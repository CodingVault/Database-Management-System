
#ifndef _ix_h_
#define _ix_h_

#include <vector>
#include <string>
#include <typeinfo>

#include "../pf/pf.h"
#include "../rm/rm.h"

# define IX_EOF (-1)  // end of the index scan

using namespace std;

typedef enum {
	SUCCESS = 0,

	// General errors
	INVALID_OPERATION = 1,
	FILE_OP_ERROR,

	// IX_Manager
	ATTRIBUTE_NOT_FOUND = 11,
	CREATE_INDEX_ERROR,
	DESTROY_INDEX_ERROR,
	OPEN_INDEX_ERROR,
	CLOSE_INDEX_ERROR,

	// IX_IndexHandle
	KEY_EXISTS = 20,	// Extra Credit
	ENTRY_NOT_FOUND,
	INSERT_ENTRY_ERROR,
	DELETE_ENTRY_ERROR,
	SEARCH_ENTRY_ERROR,
	OPEN_INDEX_HANDLE_ERROR,
	CLOSE_INDEX_HANDLE_ERROR,

	// IX_IndexScan
	END_OF_SCAN = 30,	// Not an error
	INVALID_INDEX_HANDLE = 31,
	INVALIDE_INPUT_DATA,
	OPEN_SCAN_ERROR,
	CLOSE_SCAN_ERROR,
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

	int pageNum;	// -1 indicates unsaved page	// TODO: can be 0 after metadata is used
	int leftPageNum;	// -1 means no left page; this is the most left one
	int rightPageNum;	// -1 means no right page; this is the most right one
	vector<int> childrenPageNums;
};

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
	BTree(const unsigned order, BTreeNode<KEY> *root, const unsigned height, IX_IndexHandle *ixHandle,
			BTreeNode<KEY>* (IX_IndexHandle::*func)(const unsigned, const NodeType));	// initialize a tree with given root node
	~BTree();

	RC SearchEntry(const KEY key, BTreeNode<KEY> **leafNode, unsigned &pos);
	RC InsertEntry(const KEY key, const RID &rid);
	RC DeleteEntry(const KEY key,const RID &rid);
	RC DeleteTree(BTreeNode<KEY> *Node);

	vector<BTreeNode<KEY>*> GetUpdatedNodes() const;
	vector<unsigned> GetDeletedPageNums() const;
	void ClearPendingNodes();

	BTreeNode<KEY>* GetRoot() const;
	unsigned GetHeight() const;
	KEY* GetMinKey();

protected:
	BTree();
	RC SearchNode(BTreeNode<KEY> *node, const KEY key, const unsigned height, BTreeNode<KEY> **leafNode, unsigned &pos);
	RC Insert(const KEY key, const RID &rid, BTreeNode<KEY> *leafNode, const unsigned pos);
	RC Insert(BTreeNode<KEY> *rightNode);

	RC DeleteNLeafNode(BTreeNode<KEY>* Node,unsigned nodeLevel, const KEY key, const RID &rid,int& oldchildPos);
	RC DeleteLeafNode(BTreeNode<KEY>* Node, const KEY key,const RID &rid, int& oldchildPos);

	void RedistributeNLeafNode(BTreeNode<KEY>* Node,BTreeNode<KEY>* siblingNode);
	void RedistributeLeafNode(BTreeNode<KEY>* Node,BTreeNode<KEY>* siblingNode);
	void MergeNode(BTreeNode<KEY>* leftNode,BTreeNode<KEY>* rightNode);

private:
	void InitRootNode(const NodeType nodeType);

private:
	BTreeNode<KEY>* _root;
	unsigned _order;
	unsigned _height;
	Functor<IX_IndexHandle, KEY> _func_ReadNode;

	vector<BTreeNode<KEY>*> _updated_nodes;
	vector<unsigned> _deleted_pagenums;
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

 private:
  RC InitIndexFile(const string fileName, const AttrType attrType);
 
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
  RC GetEntry(void *key, const CompOp compOp, RID &rid);
  RC GetMinKey(void *key);

  RC Open(PF_FileHandle *handle);
  RC Close();
  PF_FileHandle* GetFileHandle() const;
  AttrType GetKeyType() const;
  bool IsOpen() const;

 protected:
//  template <typename KEY>
//  BTree<KEY>* GetIndex();
  template <typename KEY>
  RC InitTree(BTree<KEY> **tree);
  template <typename KEY>
  BTreeNode<KEY>* ReadNode(const unsigned pageNum, const NodeType nodeType);
  template <typename KEY>
  RC WriteNodes(const vector<BTreeNode<KEY>*> &nodes);
  RC WriteDeletedNodes(const vector<unsigned> &_deleted_pagenums);
  RC LoadMetadata();
  template <typename KEY>
  RC UpdateMetadata(const BTree<KEY> *tree);

 private:
  template <typename KEY>
  RC GetEntry(BTree<KEY> *index, void *key, const CompOp compOp, RID &rid);
  template <typename KEY>
  RC InsertEntry(BTree<KEY> **index, void *key, const RID &rid);	// TODO: const key?
  template <typename KEY>
  RC DeleteEntry(BTree<KEY> **tree, void* key, const RID &rid);
  template <typename KEY>
  RC GetLeftEntry(const BTreeNode<KEY> *node, const unsigned pos, void *key, RID &rid);
  template <typename KEY>
  RC GetRightEntry(const BTreeNode<KEY> *node, const unsigned pos, void *key, RID &rid);

 private:
  PF_FileHandle *_pf_handle;
  AttrType _key_type;
  unsigned _height;
  unsigned _root_page_num;
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

 private:
  bool isOpen;
  char keyValue[PF_PAGE_SIZE];
  void* skipValue;
  CompOp compOp;
  IX_IndexHandle* indexHandle;
};

// print out the error message for a given return code
void IX_PrintError (RC rc);


#endif
