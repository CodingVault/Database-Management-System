
#ifndef _rm_h_
#define _rm_h_

#include <string>
#include <vector>
#include <ostream>

#include "../pf/pf.h"

using namespace std;

#define APPEND_TABLE_SUFFIX(file_name) (file_name + ".dat").c_str()
#define CATALOG_FILE_NAME "catalog.dat"
#define CATALOG_TABLE_NAME "catalog"

/* TODO: update description!!!
 * Slot count on current page is stored at the last two bytes;
 * trailing free space is initially set as 20%, excluding slot directory and control info,
 * the size of this space is stored at last four and three bytes.
 * Each slot info consists of:
 * 1) two bytes flag -- 0 free space, 1 normal data, 2 tombstone, 3 abandoned after re-organize page;
 * 2) two bytes storing offset of slot;
 * 3) two bytes storing length of slot info.
 */
#define EACH_SLOT_INFO_LEN 6
#define PAGE_CONTROL_INFO_LEN 2
#define PAGE_CONTROL_INFO_POS (PF_PAGE_SIZE - PAGE_CONTROL_INFO_LEN)
#define PAGE_PRESCRIBED_FREE_SPACE ((unsigned short) (PF_PAGE_SIZE * 0.2))

#define SLOTS_CAPACITY 1024
#define REORGANIZE_LOAD 10


// Return code
typedef int RC;

// TODO: define toString() function
// Record ID
typedef struct
{
  unsigned pageNum;
  unsigned slotNum;
} RID;


// Attribute
typedef enum { TypeInt = 0, TypeReal, TypeVarChar } AttrType;

typedef unsigned AttrLength;

struct Attribute {
    string   name;     // attribute name
    AttrType type;     // attribute type
    AttrLength length; // attribute length
};


// Comparison Operator
typedef enum { EQ_OP = 0,  // =
           LT_OP,      // <
           GT_OP,      // >
           LE_OP,      // <=
           GE_OP,      // >=
           NE_OP,      // !=
           NO_OP       // no condition
} CompOp;

/********************** NEW TYPE  **********************/
typedef struct
{
	unsigned short flag;	// 0 free space, 1 normal data, 2 tomb stone, 3 abandoned after re-organize page TODO: use enum
	unsigned short offset;
	int length;			// can be negative for the empty slot due to update on the initial free space
} Slot;
typedef struct
{
	string tableName;
	vector<Attribute> attrs;
} AttributeCache;


# define RM_EOF (-1)  // end of a scan operator

// RM_ScanIterator is an iteratr to go through records
// The way to use it is like the following:
//  RM_ScanIterator rmScanIterator;
//  rm.open(..., rmScanIterator);
//  while (rmScanIterator(rid, data) != RM_EOF) {
//    process the data;
//  }
//  rmScanIterator.close();
class RM_ScanIterator {
public:
  RM_ScanIterator();
  ~RM_ScanIterator();

  // "data" follows the same format as RM::insertTuple()
  RC getNextTuple(RID &rid, void *data);
  RC close();
  RC startReading();
  void setTableName(const string tableName);
  void setCondAttr(const string condAttribute);
  void setCompOp(const CompOp compOp);
  void setValue(const void *value);
  void setAttrNames(const vector<string> &attrNames);

private:
  int _pageNum;
  int _slotNum;
  bool _reading;
  string _tableName;
  string _conditionAttribute;
  CompOp _compOp;                  // comparison type such as "<" and "="
  void *_value;                    // used in the comparison
  vector<string> _attributeNames;  // a list of projected attributes
};

// Record Manager
class RM
{
public:
  static RM* Instance();

  RC createTable(const string tableName, const vector<Attribute> &attrs);

  RC deleteTable(const string tableName);

  RC getAttributes(const string tableName, vector<Attribute> &attrs);

  //  Format of the data passed into the function is the following:
  //  1) data is a concatenation of values of the attributes
  //  2) For int and real: use 4 bytes to store the value;
  //     For varchar: use 4 bytes to store the length of characters, then store the actual characters.
  //  !!!The same format is used for updateTuple(), the returned data of readTuple(), and readAttribute()
  RC insertTuple(const string tableName, const void *data, RID &rid);

  RC deleteTuples(const string tableName);

  RC deleteTuple(const string tableName, const RID &rid);

  // Assume the rid does not change after update
  RC updateTuple(const string tableName, const void *data, const RID &rid);

  RC readTuple(const string tableName, const RID &rid, void *data);

  RC readAttribute(const string tableName, const RID &rid, const string attributeName, void *data);

  RC reorganizePage(const string tableName, const unsigned pageNumber);

  // scan returns an iterator to allow the caller to go through the results one by one. 
  RC scan(const string tableName,
      const string conditionAttribute,
      const CompOp compOp,                  // comparision type such as "<" and "="
      const void *value,                    // used in the comparison
      const vector<string> &attributeNames, // a list of projected attributes
      RM_ScanIterator &rm_ScanIterator);


// Extra credit
public:
  RC dropAttribute(const string tableName, const string attributeName);	// TODO: update cache

  RC addAttribute(const string tableName, const Attribute attr);	// TODO: update cache

  RC reorganizeTable(const string tableName);



protected:
  RM();
  ~RM();

private:
  static RM *_rm;
  static PF_Manager *_pfManager;
  static vector<AttributeCache> _attrsCache;
  static vector<void *> _projectedTuples;

  RC initCatalog();
  RC initTableAttributes(PF_FileHandle &handle, const char *tableName, const vector<Attribute> &attrs);
  RC markDeletion(PF_FileHandle &handle, const char *tableName);
  RC loadAttributes(PF_FileHandle &handle, const char*tableName, vector<Attribute> &attrs);
  RC insert(PF_FileHandle &handle, const void *data, const unsigned size, RID &rid);
  RC read(PF_FileHandle &handle, const RID &rid, void *data, unsigned &size);
  RC update(PF_FileHandle &handle, const void *data, const unsigned size, const RID &rid);
  RC remove(PF_FileHandle &handle, const RID &rid);
  RC clearTuples(PF_FileHandle &handle);
  RC reorganizePG(PF_FileHandle &handle, const unsigned pageNumber);
  RC reoranizeTBL(PF_FileHandle &handle);
  RC appendAttribute(PF_FileHandle &handle, const char *tableName, const Attribute attr);
};

#endif
