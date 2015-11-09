/** \file lib/database.c
  sqlite3 binding
  \class Database

   provided by Peter Arthur
*/
#include "potion.h"
#include <sqlite3.h>
#include <stdio.h>

#define PN_ALLOC(V,T)        (T * volatile)potion_gc_alloc(P, V, sizeof(T))

#define PN_GET_DATABASE(t) ((struct PNDatabase *)potion_fwd((PN)t))
PNType PN_TDATABASE;

struct PNDatabase {
  PN_OBJECT_HEADER;
  sqlite3 *db;
};

struct PNCallback {
  PN_OBJECT_HEADER;
  PN_SIZE siz;
  Potion *P;
  PN     cb;
};

const int CallbackSize = sizeof(struct PNCallback) - sizeof(struct PNData);

PN potion_sqlite_open(Potion *P, PN cl, PN self, PN path);
PN potion_callback(Potion *P, PN closure);

static int
potion_database_callback(void *callback, int argc, char **argv, char **azColName) {
  struct PNCallback * cbp = (struct PNCallback *)callback;

  if (cbp != NULL) {
    vPN(Closure) cb = PN_IS_CLOSURE(cbp->cb) ? PN_CLOSURE(cbp->cb) : NULL;

    if (cb) {
      Potion *P = cbp->P;
      PN table = potion_table_empty(P);
      int i;
      for (i = 0; i < argc; i++) {
        potion_table_put(P, PN_NIL, table, PN_STR(azColName[i]),
                         argv[i] ? PN_STR(argv[i]) : PN_NIL);
      }
      // Now call the callback with the table
      cb->method(P, (PN)cb, (PN)cb, (PN)table);
    }
  }
  return 0;
}

PN potion_sqlite_new(Potion *P, PN cl, PN ign, PN path) {
  struct PNDatabase * volatile self = PN_ALLOC(PN_TDATABASE, struct PNDatabase);
  self->db = NULL;
  PN_TOUCH(self);
  return potion_sqlite_open(P, cl, (PN)self, path);
}

///\memberof Database
/// Executes the query statement and optionally calls the callback with the result.
///\param query PNString a SQL statement
///\param callback optional PNClosure with a PNTable row argument
///\return self or nil or an error
PN potion_sqlite_exec(Potion *P, PN cl, PN self, PN query, PN callback) {
  //TODO: Proper error checking
  if (!PN_IS_STR(query)) {
    return PN_NIL;
  }
  char *zErrMsg = 0;
  int rc;
  struct PNDatabase *db = (struct PNDatabase *)self;
  struct PNCallback *cb = (struct PNCallback*)potion_callback(P, callback);
  if (db->db == NULL) {
    //FIXME: Return a proper error
    return potion_io_error(P, "exec");
  }
  rc = sqlite3_exec(db->db, PN_STR_PTR(query),
                    potion_database_callback, cb, &zErrMsg);
  if (rc != SQLITE_OK) {
    // Convert to potion string and return it as an error
    fprintf(stderr, "SQL error: %s\n", zErrMsg);
    PN e = potion_io_error(P, zErrMsg);
    sqlite3_free(zErrMsg);
    return e;
  }
  return self;
}

///\memberof Database
///\param query PNString a select statement
///\return a tuple of all tables (rows)
PN potion_sqlite_gettable(Potion *P, PN cl, PN self, PN query) {
  //TODO: Proper error checking
  if (!PN_IS_STR(query)) {
    return PN_NIL;
  }
  char *q = PN_STR_PTR(query);
  struct PNDatabase *db = PN_GET_DATABASE(self);
  if (!db->db) {
    return PN_NIL;
  }
  char **pazResult;    /* Results of the query */
  int pnRow;           /* Number of result rows written here */
  int pnColumn;        /* Number of result columns written here */
  char *pzErrmsg;      /* Error msg written here */
  int rc = sqlite3_get_table(db->db, q, &pazResult,
                             &pnRow, &pnColumn, &pzErrmsg);
  // Check for error
  if (rc != SQLITE_OK) {
    // Convert to potion string and return it as an error
    fprintf(stderr, "SQL error: %s\n", pzErrmsg);
    PN e = potion_io_error(P, pzErrmsg);
    sqlite3_free(pzErrmsg);
    return e;
  }
  // Process the table
  int i, j;
  PN tuple = potion_tuple_empty(P);
  // Loop over each row
  for (i = 1; i <= pnRow; i++) {
    PN table = potion_table_empty(P);
    for (j = 0; j < pnColumn; j++) {
      char *value = pazResult[i*pnColumn + j];
      potion_table_put(P, PN_NIL, table, PN_STR(pazResult[j]),
                       value ? PN_STR(value) : PN_NIL);
    }
    PN_PUSH(tuple, table);
  }
    sqlite3_free_table(pazResult);
  return tuple;
}

PN potion_sqlite_close(Potion *P, PN cl, PN self) {
  struct PNDatabase *db = (struct PNDatabase *)self;
  if (db->db) {
    sqlite3_close(db->db);
    db->db = NULL;
  }
  return self;
}

PN potion_sqlite_open(Potion *P, PN cl, PN self, PN path) {
  struct PNDatabase *db = (struct PNDatabase *)self;
  //TODO: Proper error checking
  if (!PN_IS_STR(path)) {
    return PN_NIL;
  }
  if (db->db) {
    potion_sqlite_close(P, cl, self);
  }
  sqlite3 *sdb;
  int rc = sqlite3_open(PN_STR_PTR(path), &sdb);
  if (rc)
    return potion_io_error(P, "open");
  db->db = sdb;
  return self;
}

PN potion_sqlite_isopen(Potion *P, PN cl, PN self) {
  struct PNDatabase *db = (struct PNDatabase *)self;
  return db->db ? PN_TRUE : PN_FALSE;
}

PN potion_callback(Potion *P, PN closure) {
  struct PNCallback *cb = (struct PNCallback *)potion_data_alloc(P, CallbackSize);
  cb->siz = CallbackSize; // To help out GC
  cb->P = P;
  cb->cb = closure;
  return (PN)cb;
}

void Potion_Init_database(Potion *P) {
  PN db_vt = potion_class(P, 0, 0, 0);
  PN_TDATABASE = potion_class_type(P, db_vt);
  potion_define_global(P, PN_STR("Database"), db_vt);

  potion_type_constructor_is(db_vt, PN_FUNC(potion_sqlite_new, "path=S"));
  potion_method(db_vt, "exec", potion_sqlite_exec, "query=S|callback=o");
  potion_method(db_vt, "gettable", potion_sqlite_gettable, "query=S");
  potion_method(db_vt, "close", potion_sqlite_close, 0);
  potion_method(db_vt, "open", potion_sqlite_open, "path=S");
  potion_method(db_vt, "open?", potion_sqlite_isopen, 0);
}

/*
load "database"

db = Database("database.db")
db exec "CREATE TABLE t (id INT, name TEXT)"
db exec "INSERT INTO t (id,name) VALUES (1,'New name')"
db close
db open? say
db open("database.db")
db open? say
db exec "INSERT INTO t (id,name) VALUES (2,'Old name')"
db exec "SELECT * FROM t" (row): row say.
db gettable "SELECT * FROM t" say
db open? say
db close

*/
