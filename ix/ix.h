
#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>

#include "../pf/pf.h"
#include "../rm/rm.h"

# define IX_EOF (-1)  // end of the index scan

using namespace std;

typedef enum {
	SUCCESS = 0,
	RECORD_NOT_FOUND,
	KEY_EXISTS,

	INVALID_OPERATION = -1,
	FILE_OP_ERROR = -2,
	FILE_NOT_FOUND = -3,
} ReturnCode;

/******************** Tree Structure ********************/

#define DEFAULT_ORDER 10

typedef enum {
	NON_LEAF_NODE = 0,
	LEAF_NODE = 1,
} NodeType;

template <typename KEY>
typedef RC (*ReadNode)(BTreeNode<KEY>*, const unsigned, const NodeType);

template <typename KEY>
struct BTreeNode {
	NodeType type;
	BTreeNode<KEY>* parent;
	BTreeNode<KEY>* left;
	BTreeNode<KEY>* right;
	unsigned pos;	// the position in parent node

	vector<KEY> keys;
	vector<RID> rids;
	vector<BTreeNode<KEY>*> children;

	int pageNum;	// -1 indicates unsaved page
	int leftPageNum;	// -1 means no left page; this is the most left one
	int rightPageNum;	// -1 means no right page; this is the most right one
	vector<int> childrenPageNums;
};

template <typename KEY>
class BTree {
public:
	BTree();
	BTree(const unsigned order);
	~BTree();

	RC SearchEntry(const KEY key, BTreeNode<KEY> *leafNode, unsigned &pos);
	RC InsertEntry(const KEY key, const RID &rid);
	RC DeleteEntry(const KEY key);
	RC DeleteTree();

	RC BuildNode(const void *page);
	void SetReadNodeFunc(ReadNode func);

protected:
	RC SearchNode(const BTreeNode<KEY> *node, const KEY key, const unsigned depth, BTreeNode<KEY> *leafNode, unsigned &pos);
	RC Insert(const KEY key, const RID &rid, BTreeNode<KEY> *leafNode, const unsigned pos);
	RC Insert(const BTreeNode<KEY> *rightNode);

private:
	BTreeNode<KEY>* _root;
	unsigned _order;
	unsigned _level;
	ReadNode _func_ReadNode;
};

/******************** Tree Structure ********************/

class IX_IndexHandle;

class IX_Manager {
 public:
  static IX_Manager* Instance();

  RC CreateIndex(const string tableName,       // create new index
		 const string attributeName);
  RC DestroyIndex(const string tableName,      // destroy an index
		  const string attributeName);
  RC OpenIndex(const string tableName,         // open an index
	       const string attributeName,
	       IX_IndexHandle &indexHandle);
  RC CloseIndex(IX_IndexHandle &indexHandle);  // close index
  
 protected:
  IX_Manager   ();                             // Constructor
  ~IX_Manager  ();                             // Destructor
 
 private:
  static IX_Manager *_ix_manager;
  static PF_Manager *_pf_manager;
};


class IX_IndexHandle {
 public:
  IX_IndexHandle  ();                           // Constructor
  ~IX_IndexHandle ();                           // Destructor

  // The following two functions are using the following format for the passed key value.
  //  1) data is a concatenation of values of the attributes
  //  2) For int and real: use 4 bytes to store the value;
  //     For varchar: use 4 bytes to store the length of characters, then store the actual characters.
  RC InsertEntry(void *key, const RID &rid);  // Insert new index entry
  RC DeleteEntry(void *key, const RID &rid);  // Delete index entry

  RC Open(PF_FileHandle *handle, string keyType);
  RC Close();
  PF_FileHandle* GetFileHandle();

 protected:
  template <typename KEY>
  RC ReadNode(BTreeNode<KEY> *node, const unsigned pageNum, const NodeType type);

 private:
  PF_FileHandle *_pf_handle;
  string _key_type;
};


class IX_IndexScan {
 public:
  IX_IndexScan();  								// Constructor
  ~IX_IndexScan(); 								// Destructor

  // for the format of "value", please see IX_IndexHandle::InsertEntry()
  RC OpenScan(const IX_IndexHandle &indexHandle, // Initialize index scan
	      CompOp      compOp,
	      void        *value);           

  RC GetNextEntry(RID &rid);  // Get next matching entry
  RC CloseScan();             // Terminate index scan
};

// print out the error message for a given return code
void IX_PrintError (RC rc);


#endif