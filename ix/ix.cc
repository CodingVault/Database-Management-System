
#include <iostream>

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

/**************************************/

/********************* Tree Structure Begin *********************/

template <typename KEY>
BTree<KEY>::BTree()
{
	BTree(DEFAULT_ORDER);
}

template <typename KEY>
BTree<KEY>::BTree(const unsigned order)
{
	this->_level = 0;
	this->_order = order;

	this->_root = new BTreeNode<KEY>;
	this->_root->parent = NULL;
	this->_root->pageNum = 0;
	this->_root->leftPageNum = 1;
	this->_root->rightPageNum = 2;
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
RC BTree<KEY>::SearchNode(const BTreeNode<KEY> *node, const KEY key, const unsigned depth, BTreeNode<KEY> *leafNode, unsigned &pos)
{
	if (node->type == LEAF_NODE)		// reach leaf node
	{
		leafNode = node;
		for (unsigned index = 0; index < node->keys->size(); index++)
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
		pos = node->keys->size();	// position for insertion
		return RECORD_NOT_FOUND;
	}
	else	// non-leaf node
	{
		unsigned index = 0;
		while (index < node->keys->size())
		{
			if (node->keys[index] > key)	// <: go left; >=: go right
			{
				if (node->children[index] == NULL)
				{
					NodeType nodeType = depth > 2 ? NON_LEAF_NODE : LEAF_NODE;
					this->_func_ReadNode(node->children[index], node->childrenPageNums[index], nodeType);
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
	}
}

/**
 * Inserts <KEY, RID> pair to leaf node.
 */
template <typename KEY>
RC BTree<KEY>::Insert(const KEY key, const RID &rid, BTreeNode<KEY> *leafNode, const unsigned pos)
{
	typename vector<KEY>::iterator itKey = leafNode->keys->begin() + pos;
	leafNode->keys->insert(itKey, key);
	vector<RID>::iterator itRID = leafNode->children->begin() + pos;
	leafNode->children->insert(itRID, rid);

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
RC BTree<KEY>::Insert(const BTreeNode<KEY> *rightNode)
{
	const KEY key = rightNode->keys[0];
	BTreeNode<KEY> *parent = rightNode->parent;

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
		newNode->children = vector<RID>(parent->children.begin() + this->_order + 1, parent->children.end());
		newNode->childrenPageNums = vector<RID>(parent->childrenPageNums.begin() + this->_order + 1,
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

IX_Manager* IX_Manager::Instance()
{
	if (!_ix_manager)
		_ix_manager = new IX_Manager;
	return _ix_manager;
}

IX_Manager::IX_Manager()
{
	_pf_manager = PF_Manager::Instance();
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

RC IX_IndexHandle::InsertEntry(void *key, const RID &rid)  // Insert new index entry
{

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
	return SUCCESS;
}

PF_FileHandle* IX_IndexHandle::GetFileHandle()
{
	return this->_pf_handle;
}

template <typename KEY>
RC IX_IndexHandle::ReadNode(BTreeNode<KEY> *node, const unsigned pageNum, const NodeType type)
{
	if (strcmp(this->_key_type.c_str(), typeid(int).name()) == 0)
	{
		node = new BTreeNode<int>;
	}
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
