
#include <iostream>
#include <cassert>

#include "ix.h"

#define IX_FILE_NAME(tableName, attrName) ("IX_" + tableName + attrName + ".idx").c_str()

IX_Manager* IX_Manager::_ix_manager = 0;
PF_Manager* IX_Manager::_pf_manager = 0;

/**************************************/

bool exist(const string tableName, const string attributeName)
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

template <typename KEY>
void InitRootNode(BTreeNode<KEY> *root, const NodeType nodeType, const KEY key)
{
	root->type = nodeType;
	root->parent = NULL;
	root->left = NULL;
	root->right = NULL;
	root->pos = 0;	// must be set as 0; while splitting, its position in new root does be 0
	root->keys.push_back(key);
	root->pageNum = -1;
	root->leftPageNum = -1;
	root->rightPageNum = -1;
}

// TODO: LOW! can be re-factored as private member function to update this->_root directly
/**
 * Grows root as a leaf node.
 */
template <typename KEY>
BTreeNode<KEY>* InitRootNode(const KEY key, const RID &rid)
{
	BTreeNode<KEY> *root = new BTreeNode<KEY>;
	root->rids.push_back(rid);
	InitRootNode(root, LEAF_NODE, key);
	return root;
}

// TODO: LOW! can be re-factored as private member function to update this->_root directly
/**
 * Grows root as a new non-leaf node.
 */
template <typename KEY>
BTreeNode<KEY>* InitRootNode(const KEY key, BTreeNode<KEY> *child)
{
	BTreeNode<KEY> *root = new BTreeNode<KEY>;
	root->children.push_back(child);
	root->childrenPageNums.push_back(child->pageNum);
	InitRootNode(root, NON_LEAF_NODE, key);
	return root;
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
BTree<KEY>::BTree(const unsigned order, const KEY key, const RID &rid, IX_IndexHandle *ixHandle,
		BTreeNode<KEY>* (IX_IndexHandle::*func)(const unsigned, const NodeType))
		: _root(InitRootNode<KEY>(key, rid)), _order(order), _level(1), _func_ReadNode(ixHandle, func)
{
}

template <typename KEY>
BTree<KEY>::BTree(BTreeNode<KEY> *root, const unsigned order, const unsigned level,
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

/* ================== Helper Functions Begin ================== */

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
		this->_root = InitRootNode<KEY>(key, rightNode->left);
		parent = this->_root;
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
	if (!exist(tableName, attributeName))
	{
		IX_PrintError(INVALID_OPERATION);
		return INVALID_OPERATION;
	}
	if (_pf_manager->CreateFile(IX_FILE_NAME(tableName, attributeName)) != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}
	return SUCCESS;
}

RC IX_Manager::DestroyIndex(const string tableName,      // destroy an index
		  const string attributeName)
{
	if (_pf_manager->DestroyFile(IX_FILE_NAME(tableName, attributeName)) != SUCCESS)
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
	if (_pf_manager->OpenFile(IX_FILE_NAME(tableName, attributeName), handle) != SUCCESS)
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

/********************* IX_Manager End *********************/

/********************* IX_IndexHandle Start *********************/

IX_IndexHandle::IX_IndexHandle()
{
	this->_pf_handle = NULL;
	this->_key_type = "";
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
RC IX_IndexHandle::InitTree(BTree<KEY> *tree, const KEY key, const RID &rid)
{
	unsigned totalPageNum = this->_pf_handle->GetNumberOfPages();
	if (totalPageNum == 0)
	{
		cout << "InitTree - Initializing a tree with key [" << key << "] and rid [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
		tree = new BTree<KEY>(DEFAULT_ORDER, key, rid, this, &IX_IndexHandle::ReadNode<KEY>);
		// TODO: update this->occupancy
		// TODO: write page back
		return SUCCESS;
	}

	void *page = malloc(PF_PAGE_SIZE);
	this->_pf_handle->ReadPage(0, page);
	unsigned offset = 0;

	unsigned rootPageNum = 0;
	memcpy(&rootPageNum, (char *)page + offset, 4);
	offset += 4;
	unsigned level = 0;
	memcpy(&level, (char *)page + offset, 4);
	assert(level >= 1);		// level should be equal or greater than 1 when this function is called
	offset += 4;

	cout << "InitTree - Reading the root [at page " << rootPageNum << "] for a tree of level " << level << "." << endl;

	NodeType rootNodeType = level == 1 ? LEAF_NODE : NON_LEAF_NODE;
	BTreeNode<KEY> *root = ReadNode<KEY>(rootPageNum, rootNodeType);
	tree = new BTree<KEY>(root, DEFAULT_ORDER, level, this, &IX_IndexHandle::ReadNode<KEY>);

	// each bit in the occupancy variable represents the availability of corresponding page
	memcpy(&this->_occupancy, (char *)page + offset, 4);
	offset += 4;

	free(page);
	return SUCCESS;
}

RC IX_IndexHandle::InsertEntry(void *key, const RID &rid)  // Insert new index entry
{
	if (strcmp(this->_key_type.c_str(), typeid(int).name()) == 0)
	{
		const int intKey = *(int *)key;
		if (this->_int_index == NULL)
			InitTree(this->_int_index, intKey, rid);
		else
			this->_int_index->InsertEntry(intKey, rid);
	}
	else if (strcmp(this->_key_type.c_str(), typeid(float).name()) == 0)
	{
		const float floatKey = *(float *)key;
		if (this->_float_index == NULL)
			InitTree(this->_float_index, floatKey, rid);
		else
			this->_float_index->InsertEntry(floatKey, rid);
	}
	return SUCCESS;
}

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
