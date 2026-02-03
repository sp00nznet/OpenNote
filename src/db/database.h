#ifndef DATABASE_H
#define DATABASE_H

// Database initialization
BOOL Database_Open(const WCHAR* path);
void Database_Close(void);
BOOL Database_IsOpen(void);

// Schema management
BOOL Database_Initialize(void);
BOOL Database_Migrate(void);
int Database_GetVersion(void);
BOOL Database_SetVersion(int version);

// Utility functions
sqlite3* Database_GetHandle(void);
BOOL Database_Execute(const char* sql);
BOOL Database_BeginTransaction(void);
BOOL Database_CommitTransaction(void);
BOOL Database_RollbackTransaction(void);

// Error handling
const char* Database_GetLastError(void);

#endif // DATABASE_H
