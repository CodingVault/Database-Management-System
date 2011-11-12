
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

	ATTRIBUTE_NOT_FOUND = -4,
} ReturnCode;

class IX_IndexHandle;

/******************** Tree Structure ********************/

#define DEFAULT_ORDER 10

typedef enum {
	NON_LEAF_NODE = 0,
	LEAF_NODE = 1,
} NodeType;

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

// Read given page number as given node type to the BTreeNode
typedef BTreeNode<int>* (*ReadIntNode)(const unsigned, const NodeType);
typedef BTreeNode<float>* (*ReadFloatNode)(const unsigned, const NodeType);

template <typename Class, typename KEY>
class Functor	// TODO: use static function?
{
public:
	Functor(Class *obj, BTreeNode<KEY>* (Class::*func)(const unsigned, const NodeType)) : _obj(obj), _readNode(func) {};
	BTreeNode<KEY>* operator()(const unsigned pageNum, const NodeType nodeType) { return (*_obj.*_readNode)(pageNum, nodeType); };

private:
	Class *_obj;
	BTreeNode<KEY>* (Class::*_readNode)(const unsigned, const NodeType);
};

template <typename KEY>
class BTree {
public:
	BTree(const unsigned order, IX_IndexHandle *ixHandle, BTreeNode<KEY>* (IX_IndexHandle::*func)(const unsigned, const NodeType));	// grow a tree from the beginning
	BTree(const unsigned order, BTreeNode<KEY> *root, const unsigned level, IX_IndexHandle *ixHandle,
			BTreeNode<KEY>* (IX_IndexHandle::*func)(const unsigned, const NodeType));	// initialize a tree with given root node
	~BTree();

	RC SearchEntry(const KEY key, BTreeNode<KEY> *leafNode, unsigned &pos);
	RC InsertEntry(const KEY key, const RID &rid);
	RC DeleteEntry(const KEY key);
	RC DeleteTree();

protected:
	BTree();
	RC SearchNode(BTreeNode<KEY> *node, const KEY key, const unsigned depth, BTreeNode<KEY> *leafNode, unsigned &pos);
	RC Insert(const KEY key, const RID &rid, BTreeNode<KEY> *leafNode, const unsigned pos);
	RC Insert(BTreeNode<KEY> *rightNode);

private:
	void InitRootNode(const NodeType nodeType);

private:
	BTreeNode<KEY>* _root;
	unsigned _order;
	unsigned _level;
	Functor<IX_IndexHandle, KEY> _func_ReadNode;
};

/******************** Tree Structure ********************/

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

  RC InitMetadata(const string fileName);
 
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

  RC Open(PF_FileHandle *handle, char *keyType);
  RC Close();
  PF_FileHandle* GetFileHandle();

 protected:
  template <typename KEY>
  RC InitTree(BTree<KEY> *tree);
  template <typename KEY>
  RC InsertEntry(BTree<KEY> *tree, const KEY key, const RID &rid);
  template <typename KEY>
  BTreeNode<KEY>* ReadNode(const unsigned pageNum, const NodeType nodeType);

 private:
  PF_FileHandle *_pf_handle;
  char *_key_type;
  unsigned _free_page_num;

  BTree<int> *_int_index;
  BTree<float> *_float_index;
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
