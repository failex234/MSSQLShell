#include "stdafx.h"
#include "getopt.h"
#include <windows.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <stdio.h>
#include <sql.h>
#include <string.h>
#include <string>
#include <algorithm>
#include <vector>
#include <iostream>
#include <wchar.h>
#include <ctime>
#include <iomanip>
#include <ShlObj.h>


#define BUFSIZE 2048
#define NAME "MSSQLShell"
#define DESCRIPTION NAME " is a simple to use command line for your Microsoft SQL Server."
#define AUTHOR "Felix Naumann"
#define VERSION "0.9.5"
#define REPO "https://github.com/failex234/MSSQLShell"
#define URL "https://felixnaumann.me"

using namespace std;

HANDLE handlestdout;
SQLHANDLE sqlEnvironmentHandle, sqlConnectionHandle, sqlStatementHandle;
SQLWCHAR connectionString[2048], stateMsg[6], errorMsg[2048];
SQLINTEGER nativeError;
SQLSMALLINT charCount;
HWND desktopHandle = GetDesktopWindow();

wstring wideconnString;

wchar_t *widebuf, *returnbuf, *row;

char user[64] = { 0 };
char password[128] = { 0 };
char server[128] = { 0 };
char port[6] = { 0 };
char database[32] = { 0 };
char connString[422];

char *cmdlinename;

bool keepRunning = true;
bool clionly = false;
bool logging = false;
bool promptpassword = false;

FILE *logfile;

class TableRow {
private:
	wstring name;
	vector<wstring> columns;
	int length;
public:
	TableRow() {
		length = 0;
	}

	void append(wstring value) {
		columns.push_back(value);
		length++;
	}

	void setName(wstring newname) {
		name = newname;
	}

	void clear() {
		columns.clear();
		length = 0;
		name = L"";
	}

	void removeLast() {
		columns.pop_back();
		length--;
	}

	wstring getName() {
		return name;
	}

	wstring remove() {
		wstring temp = columns.at(length - 1);
		length--;
		columns.pop_back();
		return temp;
	}

	wstring get(int pos) {
		return columns.at(pos);
	}

	int Length() {
		return length;
	}
};

vector<TableRow> table;
vector<int> maxcharspercol;

void log(char *message) {
	if (logging) {
		logfile = fopen("log.txt", "a");

		if (logfile == NULL) {
			fprintf(stderr, "Error opening log file\n");
			return;
		}

		//Get current time
		std::time_t t = std::time(0);
		std::tm* now = std::localtime(&t);

		//dd-MM-YYY 24h masterrace
		fprintf(logfile, "[%02d/%02d/%d %02d:%02d:%02d] %s\n", now->tm_mday, now->tm_mon + 1, now->tm_year + 1900, now->tm_hour, now->tm_min, now->tm_sec, message);

		fflush(logfile);
		fclose(logfile);
	}
}

void checkForMissingArgs() {
	if (strlen(user) == 0) {
		fprintf(stderr, "A username is missing. Please specify one with -u\n");
		exit(1);
	}
	if (strlen(password) == 0) {
		promptpassword = true;
	}
	if (strlen(server) == 0) {
		fprintf(stderr, "No server was specified. Please specify one with -s\n");
		exit(1);
	}
	if (strlen(database) == 0) {
		fprintf(stderr, "No database was specified. Please specify one with -d\n");
		exit(1);
	}
	if (strlen(port) == 0) {
		strcpy(port, "1433");
	}
}

void freeEverything(int conNull, int envNull, int stmtNull) {
	if (conNull != 1) SQLDisconnect(sqlConnectionHandle);
	if (envNull != 1) SQLFreeHandle(SQL_HANDLE_ENV, sqlEnvironmentHandle);
	if (conNull != 1) SQLFreeHandle(SQL_HANDLE_DBC, sqlConnectionHandle);
	if (stmtNull != 1) SQLFreeHandle(SQL_HANDLE_STMT, sqlStatementHandle);
	//TODO Check if widebuf actually contains something!!!
	free(widebuf);
}

int longestRow() {
	int longest = 0;

	for (int i = 0; i < table.size(); i++) {
		if (table[i].Length() > longest) longest = table[i].Length();
	}

	return longest;
}

wstring getFillingSpaces(int count) {
	wstring spaces = L"";

	for (int i = 0; i < count; i++) {
		spaces += L" ";
	}

	return spaces;
}

void cls(HANDLE hConsole)
{
	COORD coordScreen = { 0, 0 };    // top-left position
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	DWORD dwConSize;

	// Get the number of character cells in the current buffer. 

	if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
	{
		return;
	}

	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

	// Fill the entire screen with blanks.

	if (!FillConsoleOutputCharacter(hConsole,        // Handle to console screen buffer 
		(TCHAR) ' ',     // Character to write to the buffer
		dwConSize,       // Number of cells to write 
		coordScreen,     // Coordinates of first cell 
		&cCharsWritten))// Receive number of characters written
	{
		return;
	}

	// Get the current text attribute.

	if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
	{
		return;
	}

	// Set the buffer's attributes accordingly.

	if (!FillConsoleOutputAttribute(hConsole,         // Handle to console screen buffer 
		csbi.wAttributes, // Character attributes to use
		dwConSize,        // Number of cells to set attribute 
		coordScreen,      // Coordinates of first cell 
		&cCharsWritten)) // Receive number of characters written
	{
		return;
	}

	// Put the cursor at its home coordinates.

	SetConsoleCursorPosition(hConsole, coordScreen);
}

int main(int argc, char **argv) {
	cmdlinename = argv[0];
	int opt;
	
	//argument checking
	while ((opt = getopt(argc, argv, "clhvu:p:s:P:d:")) != -1) {
		switch (opt)
		{
		case 'c':
			clionly = true;
			break;
		case 'l':
			logging = true;
			break;
		case 'h':
			cout << NAME << " help menu" << endl;
			cout << DESCRIPTION << endl;
			cout << "usage: " << cmdlinename << " [args]" << endl << endl;
			cout << "arguments:" << endl << "-c\t\t- enable complete CLI mode. Don't show the connection GUI" << endl;
			cout << "-u\t\t- set user id (required for cli)" << endl;
			cout << "-p\t\t- set user password" << endl;
			cout << "-P\t\t- server port (1433 is standard port)" << endl;
			cout << "-s\t\t- the server to connect to (required for cli)" << endl;
			cout << "-d\t\t- the database to use (required for cli)" << endl;
			cout << "-l\t\t- enable logging" << endl;
			cout << "-h\t\t- show this help menu" << endl;
			cout << "-v\t\t- show version information and general information about this program" << endl;
			return 0;
		case 'v':
			cout << NAME << " " << VERSION << endl;
			cout << DESCRIPTION << endl;
			cout << "Git-Repository: " << REPO << endl;
			return 0;
		case 'u':
			strcpy(user, optarg);
			break;
		case 'p':
			strcpy(password, optarg);
			break;
		case 's':
			strcpy(server, optarg);
			break;
		case 'P':
			strcpy(port, optarg);
			break;
		case 'd':
			strcpy(database, optarg);
			break;
		default:
			fprintf(stderr, "Usage: %s [-clhvupsPd] [arguments...]", argv[0]);
			return 1;
		}
	}
	if (clionly) {
		checkForMissingArgs();
		if (promptpassword) {
			printf("Password: ");
			scanf("%s", &password);
			getc(stdin);
		}

	}

	//Check if no port was given
	if (!strcmp(port, "")) {
		strcpy(port, "1433");
	}

	//Put all parameters into the connection string and then convert it to a wide string for the microsoft sql driver
	snprintf(connString, sizeof(connString), "DRIVER={SQL Server};SERVER=%s, %s;DATABASE=%s;UID=%s;PWD=%s", server, port, database, user, password);
	string tempstr = string(connString);
	wideconnString.assign(tempstr.begin(), tempstr.end());

		widebuf = (wchar_t*)calloc(sizeof(wchar_t), 1);
		//First allocate a environment handle
		if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &sqlEnvironmentHandle) != SQL_SUCCESS) {
			log("(PRE-CONNECTION) Can't allocate environment handle");
			return 1;
		}

		//Set the environment attributes for the connection
		if (SQLSetEnvAttr(sqlEnvironmentHandle, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0) != SQL_SUCCESS) {
			log("(PRE-CONNECTION) Can't set SQL environment attributes");
			return 1;
		}

		//allocate a connection handle
		if (SQLAllocHandle(SQL_HANDLE_DBC, sqlEnvironmentHandle, &sqlConnectionHandle) != SQL_SUCCESS) {
			log("(PRE-CONNECTION) Can't allocate SQL connection handle");
			return 1;
		}

		//establish a connection
		SQLRETURN retcode;
		retcode = SQLDriverConnect(sqlConnectionHandle, desktopHandle, (SQLWCHAR*)&wideconnString[0], SQL_NTS, connectionString, 2048, NULL, (clionly == false ? SQL_DRIVER_PROMPT : SQL_DRIVER_NOPROMPT));
		if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
			puts("Connection failed!");

			//Get error
			SQLGetDiagRec(SQL_HANDLE_DBC, sqlConnectionHandle, 1, stateMsg, &nativeError, errorMsg, 2048, &charCount);
			if (wcslen(errorMsg) > 1 && wcslen(stateMsg) > 1) {
				wprintf(L"%ls (%ls)\n", errorMsg, stateMsg);

				char logmsg[2048] = "Connection failed with error \"\"";
				log(logmsg);
			}
			else {
				log("Connection failed with error \"User aborted connection\"");
			}

			freeEverything(1, 0, 1);
			puts("Press any key to exit...");
			getc(stdin);
			return 1;
		}
		log("Connection established");
		puts("Connection successfully established!");
		handlestdout = GetStdHandle(STD_OUTPUT_HANDLE);

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
			else if (!wcscmp(widebuf, L"clear") || !wcscmp(widebuf, L"cls")) {
				cls(handlestdout);
			}
			else {
				log("Run statement");

				retcode = SQLExecDirect(sqlStatementHandle, (SQLWCHAR*)widebuf, SQL_NTS);
				//Query not successful. Output error message and number
				if (retcode != SQL_SUCCESS && retcode != SQL_SUCCESS_WITH_INFO) {
					retcode = SQLGetDiagRec(SQL_HANDLE_STMT, sqlStatementHandle, 1, stateMsg, &nativeError, errorMsg, 2048, &charCount);

					wprintf(L"%ls (%ls)\n", errorMsg, stateMsg);
				}
				else {
					//Loop through all rows and add them to our vector. Also save the the length of the longest string of each column
					while (SQLFetch(sqlStatementHandle) == SQL_SUCCESS) {
						returnbuf = (wchar_t*)malloc(sizeof(wchar_t) * BUFSIZE);

						int i = 1;
						int prevcode;
						retcode = (SQLGetData(sqlStatementHandle, i, SQL_WCHAR, returnbuf, BUFSIZE, NULL));
						TableRow row;
						while (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_NULL_DATA) {
							//Save the previous return code to know when no more columns are coming
							prevcode = retcode;
							row.append((retcode != SQL_NULL_DATA ? returnbuf : L"    "));
							i++;
							retcode = (SQLGetData(sqlStatementHandle, i, SQL_WCHAR, returnbuf, BUFSIZE, NULL));
							if (prevcode == retcode && retcode == -1) break;
						}
						//Add the row to the table
						table.push_back(row);

						free(returnbuf);
					}

					if (table.size() == 0) continue;
					//Remove all trailing nulls
					for (int i = 0; i < table.size(); i++) {
						table[i].removeLast();
					}

					//Fill the vector that saves the length of the longest string in each column with zeroes
					for (int i = 0; i < table[0].Length(); i++) {
						maxcharspercol.push_back(0);
					}

					//Get the length of the longest string in each column
					for (int i = 0; i < table.size(); i++) {
						for (int j = 0; j < table[i].Length(); j++) {
							if (j > maxcharspercol.size() - 1) maxcharspercol.push_back(0);
							if (wcslen(&table[i].get(j)[0]) > maxcharspercol[j]) maxcharspercol[j] = wcslen(&table[i].get(j)[0]);
						}
					}
					if (table[0].Length() != 2) {
						//Print the table header
						for (int i = 0; i < maxcharspercol.size(); i++) {
							cout << "+";
							for (int j = 0; j < maxcharspercol[i] + 2; j++) {
								cout << "-";
							}
						}
						cout << "+" << endl;
					}

					//Print each field and divide them with a "|"
					for (int i = 0; i < table.size(); i++) {
						cout << "| ";
						for (int j = 0; j < table[i].Length(); j++) {
							wcout << left;
							wcout << setw(maxcharspercol[j]) << table[i].get(j) << L" | ";

							if (j + 1 == table[i].Length()) {
								if (table[i].Length() < longestRow()) {
									for (int k = 1; k < (longestRow() - table[i].Length()) + 1; k++) {
										wcout << left;
										wcout << getFillingSpaces(maxcharspercol[j + k]) << L" | ";
									}
								}
							}
						}
						putc('\n', stdout);
					}

					if (table[0].Length() != 2) {
						//Print the table footer
						for (int i = 0; i < maxcharspercol.size(); i++) {
							cout << "+";
							for (int j = 0; j < maxcharspercol[i] + 2; j++) {
								cout << "-";
							}
						}
						cout << "+" << endl;
					}
				}
			}
			maxcharspercol.clear();
			table.clear();


		}
		//free all handles and pointers
		freeEverything(0, 0, 0);
		log("Disconnected from database");

		return 0;
	}