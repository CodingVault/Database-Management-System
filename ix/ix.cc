
#include <iostream>

#include "ix.h"

#define IX_FILE_NAME(tableName, attrName) ("IX_" + tableName + attrName + ".idx")

IX_Manager* IX_Manager::_ix_manager = 0;
PF_Manager* IX_Manager::_pf_manager = 0;

/**************************************/

bool exist(const string fileName)
{
	// TODO: Implementation!
	return true;
}

template <typename KEY>
unsigned binarySearch(const vector<KEY> keys, const KEY key)
{
	// TODO: Implementation!
	return -1;
}

// TODO: Implementation!
/**
 * Writes node data to file, and update page number for the node.
 */
template <typename KEY>
void writeNode(PF_FileHandle &handle, const BTreeNode<KEY> *node)
{
	// TODO: update current node pageNum and parent's childrenPageNum

	if (node->type == NodeType(0))
	{
		// write non-leaf node, and update page number

		for (unsigned index = 0; index < node->childrenPageNums.size(); index++)
		{
			if (node->childrenPageNums[index] == -1)
			{
				writeNode(handle, node->children[index]);
			}
		}
	}
	else
	{
		// write leaf node, and update page number
	}

	return 0;
}

/**************************************/

/********************* Tree Structure Begin *********************/

template <typename KEY>
BTree<KEY>::BTree()
{
	KEY key;
	RID rid;
	rid.pageNum = 0;
	rid.slotNum = 0;
	BTree(DEFAULT_ORDER, key, rid);
}

template <typename KEY>
BTree<KEY>::BTree(const unsigned order, IX_IndexHandle *ixHandle,
		BTreeNode<KEY>* (IX_IndexHandle::*func)(const unsigned, const NodeType))
		: _order(order), _level(1), _func_ReadNode(ixHandle, func)
{
	InitRootNode(LEAF_NODE);
}

template <typename KEY>
BTree<KEY>::BTree(const unsigned order, BTreeNode<KEY> *root, const unsigned level,
		IX_IndexHandle *ixHandle, BTreeNode<KEY>* (IX_IndexHandle::*func)(const unsigned, const NodeType))
		: _root(root), _order(order), _level(level), _func_ReadNode(ixHandle, func)
{
}

template <typename KEY>
BTree<KEY>::~BTree()
{
	DeleteTree();
}

/* ================== Helper Functions Begin ================== */

template <typename KEY>
void split(const unsigned order, BTreeNode<KEY> *splitee, BTreeNode<KEY> *newNode)
{
	// construct new node -- the right one
	newNode->parent = splitee->parent;
	newNode->left = splitee;
	newNode->right = splitee->right;
	newNode->keys = vector<KEY>(splitee->keys.begin() + order, splitee->keys.end());
	newNode->pageNum = -1;
	newNode->pos = splitee->pos + 1;

	// update old node -- the left one
	splitee->right = newNode;
	splitee->keys.resize(order);
}

/* ================== Helper Functions End ================== */

/* ================== Private Functions Begin ================== */

template <typename KEY>
void BTree<KEY>::InitRootNode(NodeType nodeType)
{
	this->_root = new BTreeNode<KEY>;
	this->_root->type = nodeType;
	this->_root->parent = NULL;
	this->_root->left = NULL;
	this->_root->right = NULL;
	this->_root->pos = 0;	// must be set as 0; while splitting, its position in new root does be 0
	this->_root->pageNum = -1;
	this->_root->leftPageNum = -1;
	this->_root->rightPageNum = -1;
}

/* ================== Private Functions End ================== */

/* ================== Protected Functions Begin ================== */

/**
 * Looks for the leaf node in which the given key should exist.
 *
 * @return pos is the position of the given key in leaf node, or position for insertion if not found.
 */
template <typename KEY>
RC BTree<KEY>::SearchNode(BTreeNode<KEY> *node, const KEY key, const unsigned depth, BTreeNode<KEY> *leafNode, unsigned &pos)
{
	if (node->type == LEAF_NODE)		// reach leaf node
	{
		leafNode = node;
		for (unsigned index = 0; index < node->keys.size(); index++)
		{
			if (key == node->keys[index])
			{
				pos = index;
				return SUCCESS;
			}
			else if (node->keys[index] > key)
			{
				pos = index;	// position for insertion
				return RECORD_NOT_FOUND;
			}
		}
		pos = node->keys.size();	// position for insertion
		return RECORD_NOT_FOUND;
	}
	else	// non-leaf node
	{
		unsigned index = 0;
		while (index < node->keys.size())
		{
			if (node->keys[index] > key)	// <: go left; >=: go right
			{
				if (node->children[index] == NULL)
				{
					NodeType nodeType = depth > 2 ? NON_LEAF_NODE : LEAF_NODE;
					node->children[index] = this->_func_ReadNode(node->childrenPageNums[index], nodeType);
				}
				SearchNode(node->children[index], key, depth - 1, leafNode, pos);
			}
			else
			{
				index++;
				continue;
			}
		}
		SearchNode(node->children[index], key, depth - 1, leafNode, pos);	// index = node->keys->size() = node->children->size() - 1
		return SUCCESS;
	}
}

/**
 * Inserts <KEY, RID> pair to leaf node.
 */
template <typename KEY>
RC BTree<KEY>::Insert(const KEY key, const RID &rid, BTreeNode<KEY> *leafNode, const unsigned pos)
{
	typename vector<KEY>::iterator itKey = leafNode->keys.begin() + pos;
	leafNode->keys.insert(itKey, key);
	vector<RID>::iterator itRID = leafNode->rids.begin() + pos;
	leafNode->rids.insert(itRID, rid);

	if (leafNode->keys.size() > this->_order * 2)	// need to split
	{
		BTreeNode<KEY> *newNode = new BTreeNode<KEY>;
		split(this->_order, leafNode, newNode);

		// update old node -- the left one
		leafNode->rids.resize(this->_order);

		// update new node -- the right one
		newNode->type = LEAF_NODE;
		newNode->rids = vector<RID>(leafNode->rids.begin() + this->_order, leafNode->rids.end());

		// update parent
		this->Insert(newNode);
	}

	return SUCCESS;
}

/**
 * Inserts split node to parent node; recursive to parent's ancestor nodes if necessary.
 */
template <typename KEY>
RC BTree<KEY>::Insert(BTreeNode<KEY> *rightNode)
{
	const KEY key = rightNode->keys[0];
	BTreeNode<KEY> *parent = rightNode->parent;

	if (parent == NULL)		// reach root
	{
		InitRootNode(NON_LEAF_NODE);
		this->_root->keys.push_back(key);
		this->_root->children.push_back(rightNode->left);
		this->_root->childrenPageNums.push_back(rightNode->left->pageNum);
		parent = this->_root;
		this->_level++;
	}

	// update parent node
	typename vector<KEY>::iterator itKey = parent->keys.begin() + rightNode->pos;
	parent->keys.insert(itKey, key);

	typename vector<BTreeNode<KEY>*>::iterator itChildren = parent->children.begin() + rightNode->pos;
	parent->children.insert(itChildren, rightNode);

	vector<int>::iterator itChildrenPageNums = parent->childrenPageNums.begin() + rightNode->pos;
	parent->childrenPageNums.insert(itChildrenPageNums, rightNode->pageNum);	// insert -1 in fact

	if (parent->keys.size() > this->_order * 2)	// need to split
	{
		BTreeNode<KEY> *newNode = new BTreeNode<KEY>;
		split(this->_order, parent, newNode);

		// update old node -- the left one
		parent->children.resize(this->_order + 1);
		parent->childrenPageNums.resize(this->_order + 1);

		// update new node -- the right one
		newNode->type = NON_LEAF_NODE;
		newNode->children.insert(newNode->children.begin(), parent->children.begin() + this->_order + 1, parent->children.end());
		newNode->childrenPageNums.insert(newNode->childrenPageNums.begin(),
				parent->childrenPageNums.begin() + this->_order + 1,
				parent->childrenPageNums.end());

		// update parent
		this->Insert(newNode);
	}

	return SUCCESS;
}

/* ================== Protected Functions End ================== */

/* ================== Public Functions End ================== */

template <typename KEY>
RC BTree<KEY>::SearchEntry(const KEY key, BTreeNode<KEY> *leafNode, unsigned &pos)
{
	return SearchNode(this->_root, key, this->_level, leafNode, pos);
}

template <typename KEY>
RC BTree<KEY>::InsertEntry(const KEY key, const RID &rid)
{
	BTreeNode<KEY> *leafNode;
	unsigned pos;
	RC rc = SearchEntry(key, leafNode, pos);

	if (rc == SUCCESS)
	{
		IX_PrintError(KEY_EXISTS);		// TODO: Extra Credit -- Overflow
		return KEY_EXISTS;
	}
	else
	{
		Insert(key, rid, leafNode, pos);	// TODO: Error Handling
	}

	return SUCCESS;
}

template <typename KEY>
RC BTree<KEY>::DeleteTree()
{
	return SUCCESS;
}

/* ================== Public Functions End ================== */

/********************* Tree Structure End *********************/

/********************* IX_Manager Begin *********************/

IX_Manager::IX_Manager()
{
	_pf_manager = PF_Manager::Instance();
}

IX_Manager* IX_Manager::Instance()
{
	if (!_ix_manager)
		_ix_manager = new IX_Manager;
	return _ix_manager;
}

RC IX_Manager::CreateIndex(const string tableName,       // create new index
		 const string attributeName)
{
	const string fileName = IX_FILE_NAME(tableName, attributeName);
	if (!exist(fileName))
	{
		IX_PrintError(INVALID_OPERATION);
		return INVALID_OPERATION;
	}
	if (_pf_manager->CreateFile(fileName.c_str()) != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}

	InitMetadata(fileName);
	return SUCCESS;
}

RC IX_Manager::DestroyIndex(const string tableName,      // destroy an index
		  const string attributeName)
{
	if (_pf_manager->DestroyFile(IX_FILE_NAME(tableName, attributeName).c_str()) != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}
	return SUCCESS;
}

RC IX_Manager::OpenIndex(const string tableName,         // open an index
	       const string attributeName,
	       IX_IndexHandle &indexHandle)
{
	PF_FileHandle handle;
	if (_pf_manager->OpenFile(IX_FILE_NAME(tableName, attributeName).c_str(), handle) != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}
	string keyType;		// TODO: get key type
	indexHandle.Open(&handle, keyType);
	return SUCCESS;
}

RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)  // close index
{
	PF_FileHandle *handle = indexHandle.GetFileHandle();
	if (_pf_manager->CloseFile(*handle) != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}
	indexHandle.Close();
	return SUCCESS;
}

RC IX_Manager::InitMetadata(string fileName)
{
	PF_FileHandle handle;
	if (_pf_manager->OpenFile(fileName.c_str(), handle) != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}

	unsigned level = 0;
	unsigned rootPageNum = 0;	// indicates root does not exist yet
	unsigned freePageNum = 0;	// indicates no free page now
	void *page = malloc(PF_PAGE_SIZE);
	memcpy(page, &rootPageNum, 4);
	memcpy((char *)page + 4, &level, 4);
	memcpy((char *)page + 8, &freePageNum, 4);
	handle.AppendPage(page);
	free(page);

	if (_pf_manager->CloseFile(handle) != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}
	return SUCCESS;
}

/********************* IX_Manager End *********************/

/********************* IX_IndexHandle Begin *********************/

IX_IndexHandle::IX_IndexHandle()
{
	this->_pf_handle = NULL;
	this->_key_type = "";
	this->_free_page_num = 0;
	this->_int_index = NULL;
	this->_float_index = NULL;
}

IX_IndexHandle::~IX_IndexHandle()
{
	if (this->_int_index != NULL)
		delete this->_int_index;
	if (this->_float_index != NULL)
		delete this->_float_index;
}

/* ================== Public Functions Begin ================== */

template <typename KEY>
BTreeNode<KEY>* IX_IndexHandle::ReadNode(const unsigned pageNum, const NodeType type)
{
	void *page = malloc(PF_PAGE_SIZE);
//	this->_pf_handle->ReadPage(pageNum, page);
	BTreeNode<KEY> *node;

//	if (type == LEAF_NODE)

	free(page);
	return node;
}

template <typename KEY>
RC IX_IndexHandle::InitTree(BTree<KEY> *tree)
{
	void *page = malloc(PF_PAGE_SIZE);
	this->_pf_handle->ReadPage(0, page);
	unsigned offset = 0;

	unsigned rootPageNum = 0;
	memcpy(&rootPageNum, (char *)page + offset, 4);
	offset += 4;
	unsigned level = 0;
	memcpy(&level, (char *)page + offset, 4);
	offset += 4;
	unsigned freePageNum = 0;
	memcpy(&freePageNum, (char *)page + offset, 4);
	free(page);

	if (level > 0)
	{
		cout << "InitTree - Reading the root [at page " << rootPageNum << "] for a tree of level " << level << "." << endl;
		NodeType rootNodeType = level == 1 ? LEAF_NODE : NON_LEAF_NODE;
		BTreeNode<KEY> *root = ReadNode<KEY>(rootPageNum, rootNodeType);
		tree = new BTree<KEY>(DEFAULT_ORDER, root, level, this, &IX_IndexHandle::ReadNode<KEY>);
	}
	else
	{
		cout << "InitTree - Initializing a tree with empty root as a leaf node." << endl;
		tree = new BTree<KEY>(DEFAULT_ORDER, this, &IX_IndexHandle::ReadNode<KEY>);
	}
	this->_free_page_num = freePageNum;

	return SUCCESS;
}

template <typename KEY>
RC IX_IndexHandle::InsertEntry(BTree<KEY> *tree, const KEY key, const RID &rid)
{
	if (tree == NULL)
		this->InitTree(tree);

	return tree->InsertEntry(key, rid);
}

RC IX_IndexHandle::InsertEntry(void *key, const RID &rid)  // Insert new index entry
{
	RC rc;
	if (strcmp(this->_key_type.c_str(), typeid(int).name()) == 0)
	{
		const int intKey = *(int *)key;
		rc = InsertEntry(this->_int_index, intKey, rid);
	}
	else if (strcmp(this->_key_type.c_str(), typeid(float).name()) == 0)
	{
		const float floatKey = *(float *)key;
		rc = InsertEntry(this->_float_index, floatKey, rid);
	}
	return rc;
}

/* ================== Public Functions End ================== */

RC IX_IndexHandle::Open(PF_FileHandle *handle, string keyType)
{
	if (this->_pf_handle != NULL)
	{
		IX_PrintError(INVALID_OPERATION);
		return INVALID_OPERATION;
	}

	this->_pf_handle = handle;
	this->_key_type = keyType;

	return SUCCESS;
}

RC IX_IndexHandle::Close()
{
	if (this->_pf_handle == NULL)
	{
		IX_PrintError(INVALID_OPERATION);
		return INVALID_OPERATION;
	}

	this->_pf_handle = NULL;
	delete this->_int_index;
	delete this->_float_index;
	return SUCCESS;
}

PF_FileHandle* IX_IndexHandle::GetFileHandle()
{
	return this->_pf_handle;
}

/********************* IX_IndexHandle End *********************/

/********************* IX_IndexScan Start *********************/



/********************* IX_IndexScan End *********************/

void IX_PrintError(RC rc)
{
	string errMsg = "";

//	switch (rc)
//	{
//
//	}

	cout << "Encountered error: " << errMsg << endl;
}
