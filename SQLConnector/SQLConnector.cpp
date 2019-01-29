#include "stdafx.h"
#include <windows.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <stdio.h>
#include <sql.h>
#include <string.h>

#define BUFSIZE 2048

using namespace std;

SQLHANDLE sqlEnvironmentHandle, sqlConnectionHandle, sqlStatementHandle;
SQLWCHAR connectionString[2048], stateMsg[6], errorMsg[2048];
SQLINTEGER nativeError;
SQLSMALLINT charCount;
HWND desktopHandle = GetDesktopWindow();

wchar_t *widebuf, *returnbuf;
bool keepRunning = true;

void freeEverything(int conNull, int envNull, int stmtNull) {
	if (conNull != 1) SQLDisconnect(sqlConnectionHandle);
	if (envNull != 1) SQLFreeHandle(SQL_HANDLE_ENV, sqlEnvironmentHandle);
	if (conNull != 1) SQLFreeHandle(SQL_HANDLE_DBC, sqlConnectionHandle);
	if (stmtNull != 1) SQLFreeHandle(SQL_HANDLE_STMT, sqlStatementHandle);
	free(widebuf);
}

int main(int argc, char **argv) {
	widebuf = (wchar_t*) calloc(sizeof(wchar_t), 1);
	//First allocate a environment handle
	if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &sqlEnvironmentHandle) != SQL_SUCCESS) {
		puts("unable to allocate environment handle");
		return 1;
	}

	//Set the environment attributes for the connection
	if (SQLSetEnvAttr(sqlEnvironmentHandle, SQL_ATTR_ODBC_VERSION, (SQLPOINTER) SQL_OV_ODBC3, 0) != SQL_SUCCESS) {
		puts("unable to set environment attributes");
		return 1;
	}

	//allocate a connection handle
	if (SQLAllocHandle(SQL_HANDLE_DBC, sqlEnvironmentHandle, &sqlConnectionHandle) != SQL_SUCCESS) {
		puts("unable to allocate connection handle");
		return 1;
	}

	//establish a connection
	SQLRETURN retcode;
	retcode = SQLDriverConnect(sqlConnectionHandle, desktopHandle, (SQLWCHAR*)L"DRIVER={SQL Server};SERVER=127.0.0.1, 1433;DATABASE=TIREESTD;UID=user;PWD=password", SQL_NTS, connectionString, 2048, NULL, SQL_DRIVER_PROMPT);
	if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
		puts("Connection failed");
		freeEverything(1, 0, 1);
		return 1;
	}
		puts("Connection successfully established!");

		while (keepRunning) {
			//allocate a statement handle to be able to query
			if (SQLAllocHandle(SQL_HANDLE_STMT, sqlConnectionHandle, &sqlStatementHandle) != SQL_SUCCESS) {
				puts("Can't create statement handle");
				freeEverything(0, 0, 1);
			}

			//allocate space for the buffer
			widebuf = (wchar_t*)malloc(sizeof(wchar_t) * BUFSIZE);

			printf("Query> ");

			//read input and remove newline
			fgetws(widebuf, BUFSIZE, stdin);
			widebuf[wcscspn(widebuf, L"\n")] = L'\0';

			if (!wcscmp(widebuf, L"exit")) {
				keepRunning = false;
			}
			else {
				retcode = SQLExecDirect(sqlStatementHandle, (SQLWCHAR*)widebuf, SQL_NTS);
				//Query not successful. Output error message and number
				if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
					retcode = SQLGetDiagRec(SQL_HANDLE_STMT, sqlStatementHandle, 1, stateMsg, &nativeError, errorMsg, 2048, &charCount);

					wprintf(L"%ls (%ls)\n", errorMsg, stateMsg);
				}
				else {
					//Loop through all results and output them
					while (SQLFetch(sqlStatementHandle) == SQL_SUCCESS) {
						returnbuf = (wchar_t*)malloc(sizeof(wchar_t) * BUFSIZE);

						SQLGetData(sqlStatementHandle, 1, SQL_WCHAR, returnbuf, BUFSIZE, NULL);
						wprintf(L"%ls\n", returnbuf);

						free(returnbuf);
					}
				}
			}


		}
		//free all handles and pointers
		freeEverything(0, 0, 0);

		return 0;
}