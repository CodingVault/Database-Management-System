
#include <iostream>

#include "ix.h"

#define IX_FILE_NAME(tableName, attrName) ("IX_" + tableName + "_" + attrName + ".idx")
#define DEBUG true

IX_Manager* IX_Manager::_ix_manager = 0;
PF_Manager* IX_Manager::_pf_manager = 0;

/**************************************/

bool Exist(const string fileName)
{
	// TODO: Implementation!
	return true;
}

template <typename KEY>
unsigned BinarySearch(const vector<KEY> keys, const KEY key)
{
	// TODO: Implementation!
	return -1;
}

template <typename KEY>
void PrintNode(const BTreeNode<KEY> *node)
{
	cout << "====================================================" << endl;
	cout << "Print node information as " << (node->type == NodeType(1) ? "leaf node." : "non-leaf node.") << endl;
	cout << "(Page #: " << node->pageNum << ", ";
	cout << "Key(s) #: " << node->keys.size() << ", ";
	cout << "Parent: " << (node->parent == NULL ? -1 : node->parent->pageNum) << ", ";
	if (node->type == NodeType(1))	// leaf node
	{
		cout << "Left: " << node->leftPageNum << ", ";
		cout << "Right: " << node->rightPageNum << ", ";

		for (unsigned index = 0; index < node->keys.size(); index++)
			cout << "(" << index << ") " << node->keys[index] << "->[" << node->rids[index].pageNum << ":" << node->rids[index].slotNum << "], ";
	}
	else	// non-leaf node
	{
		for (unsigned index = 0; index < node->keys.size(); index++)
			cout << "[" << node->childrenPageNums[index] << "] (" << node->keys[index] << ") ";
		if (node->childrenPageNums.size() > 1)
			cout << "[" << node->childrenPageNums[node->childrenPageNums.size() - 1] << "]";
	}
	cout << ")" << endl;
	cout << "====================================================" << endl;
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
		: _order(order), _height(1), _func_ReadNode(ixHandle, func)
{
	InitRootNode(NodeType(1));
}

template <typename KEY>
BTree<KEY>::BTree(const unsigned order, BTreeNode<KEY> *root, const unsigned level,
		IX_IndexHandle *ixHandle, BTreeNode<KEY>* (IX_IndexHandle::*func)(const unsigned, const NodeType))
		: _root(root), _order(order), _height(level), _func_ReadNode(ixHandle, func)
{
}

template <typename KEY>
BTree<KEY>::~BTree()
{
	DeleteTree();
}

/* ================== Helper Functions Begin ================== */

template <typename KEY>
void Split(const unsigned order, BTreeNode<KEY> *splitee, BTreeNode<KEY> *newNode)
{
	// construct new node -- the right one
	newNode->parent = splitee->parent;
	newNode->left = splitee;
	newNode->right = splitee->right;
	newNode->pageNum = -1;
	newNode->pos = splitee->pos + 1;
	newNode->leftPageNum = splitee->pageNum;
	newNode->rightPageNum = splitee->rightPageNum;

	// update old node -- the left one
	splitee->right = newNode;
	splitee->rightPageNum = newNode->pageNum;
}

/* ================== Helper Functions End ================== */

/* ================== Private Functions Begin ================== */

template <typename KEY>
void BTree<KEY>::InitRootNode(const NodeType nodeType)
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
 * @param pos is the position of the given key in leaf node, or position for insertion if not found.
 */
template <typename KEY>
RC BTree<KEY>::SearchNode(BTreeNode<KEY> *node, const KEY key, const unsigned height, BTreeNode<KEY> **leafNode, unsigned &pos)
{
	if (DEBUG)
		cout << "BTree<KEY>::SearchNode - Search node [" << node->pageNum << "] at height " << height << "." << endl;

	if (node->type == NodeType(1))		// reach leaf node
	{
		*leafNode = node;
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
					NodeType nodeType = height > 2 ? NodeType(0) : NodeType(1);
					node->children[index] = this->_func_ReadNode(node->childrenPageNums[index], nodeType);
					BTreeNode<KEY> *parent = node;
					node->children[index]->parent = parent;	// NOTE: remember node->children[index] is a pointer; use "->" to refer to its fields
					node->children[index]->pos = index;
				}
				return SearchNode(node->children[index], key, height - 1, leafNode, pos);
			}

			index++;
		}
		return SearchNode(node->children[index], key, height - 1, leafNode, pos);	// index = node->keys->size() = node->children->size() - 1
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
	this->_updated_nodes.push_back(leafNode);

	if (leafNode->keys.size() > this->_order * 2)	// need to split
	{
		BTreeNode<KEY> *newNode = new BTreeNode<KEY>;
		Split(this->_order, leafNode, newNode);

		// update new node -- the right one
		newNode->type = NodeType(1);
		newNode->keys = vector<KEY>(leafNode->keys.begin() + this->_order, leafNode->keys.end());
		newNode->rids = vector<RID>(leafNode->rids.begin() + this->_order, leafNode->rids.end());

		// update old node -- the left one
		leafNode->keys.resize(this->_order);
		leafNode->rids.resize(this->_order);

		// update parent
		return this->Insert(newNode->keys[0], newNode);
	}

	return SUCCESS;
}

/**
 * Inserts split node to parent node; recursive to parent's ancestor nodes if necessary.
 */
template <typename KEY>
RC BTree<KEY>::Insert(const KEY key, BTreeNode<KEY> *rightNode)
{
	if (rightNode->parent == NULL)		// reach root
	{
		InitRootNode(NodeType(0));
		this->_root->children.push_back(rightNode->left);
		this->_root->childrenPageNums.push_back(rightNode->left->pageNum);
		rightNode->parent = this->_root;
		rightNode->left->parent = this->_root;
		this->_height++;

		this->_updated_nodes.push_back(rightNode);
		this->_updated_nodes.push_back(rightNode->left);	// update its parent
	}
	BTreeNode<KEY> *parent = rightNode->parent;

	// update parent node
	typename vector<KEY>::iterator itKey = parent->keys.begin() + rightNode->pos - 1;
	parent->keys.insert(itKey, key);

	typename vector<BTreeNode<KEY>*>::iterator itChildren = parent->children.begin() + rightNode->pos;
	parent->children.insert(itChildren, rightNode);

	vector<int>::iterator itChildrenPageNums = parent->childrenPageNums.begin() + rightNode->pos;
	parent->childrenPageNums.insert(itChildrenPageNums, rightNode->pageNum);	// insert -1 in fact

	RC rc = SUCCESS;
	if (parent->keys.size() > this->_order * 2)	// need to split
	{
		BTreeNode<KEY> *newNode = new BTreeNode<KEY>;
		Split(this->_order, parent, newNode);

		// update new node -- the right one
		newNode->type = NodeType(0);
		newNode->keys = vector<KEY>(parent->keys.begin() + this->_order + 1, parent->keys.end());
		newNode->children.insert(newNode->children.begin(), parent->children.begin() + this->_order + 1, parent->children.end());
		newNode->childrenPageNums.insert(newNode->childrenPageNums.begin(),
				parent->childrenPageNums.begin() + this->_order + 1,
				parent->childrenPageNums.end());

		// update old node -- the left one
		parent->keys.resize(this->_order);
		parent->children.resize(this->_order + 1);
		parent->childrenPageNums.resize(this->_order + 1);

		// update parent
		rc = this->Insert(parent->keys[this->_order], newNode);
	}
	this->_updated_nodes.push_back(parent);

	return rc;
}

/*
 * redistribute elements evenly between the non-leaf node and its sibling
 * assume there are more elements in siblingNode to spare
 */
template <typename KEY>
RC BTree<KEY>::redistribute_NLeafNode(BTreeNode<KEY>* Node,BTreeNode<KEY>* siblingNode)
{
    int i = 0;
    int even_no = (siblingNode->keys.size() + Node->keys.size())/2;
	if(Node->Pos > siblingNode->Pos)
	{// left sibling
		// insert the key value of parent into Node
		Node->keys.insert(Node->keys.begin(),Node->parent->keys[siblingNode->pos]);
		// update the key value of parent
		Node->parent->keys[siblingNode->Pos] = siblingNode->keys[even_no];

		// move keys from siblingNode to Node
		for(i = siblingNode->keys.size()-1; i > even_no; i--)
		{
		    Node->keys.insert(Node->keys.begin(),siblingNode->keys[i]);
		}
		// move children pageNums and children pointers from siblingNode to Node
		for(i = siblingNode->keys.size()-1; i >= even_no; i--)
		{
			Node->childrenPageNums.insert(Node->childrenPageNums.begin(),
					    		    	siblingNode->childrenPageNums[i]);
			Node->children.insert(Node->children.begin(),siblingNode->children[i]);
		}

		//delete the keys in siblingNode
		while(siblingNode->keys.size() > even_no)
		{
			siblingNode->keys.erase(siblingNode->keys.end());
		}
		//delete the childrenPageNums and children pointers in siblingNode
		while(siblingNode->childrenPageNums.size() > even_no)
		{
			siblingNode->childrenPageNums.erase(siblingNode->childrenPageNums.end());
			siblingNode->children.erase(siblingNode->children.end());
		}
	}
	else
	{// right sibling
		even_no = (siblingNode->keys.size()- Node->keys.size())/2;
		// insert the key value of parent into Node
		Node->keys.push_back(Node->parent->keys[Node->pos]);
		// update the key value of parent
		Node->parent->keys[Node->Pos] = siblingNode->keys[even_no-1];

		// move keys from siblingNode to Node
		i = 0;
		while(i < even_no - 1)
		{
		    Node->keys.push_back(siblingNode->keys[i]);
		    i++;
		}

		// move children pageNums and children pointers from siblingNode to Node
		i = 0;
		while(i < even_no)
		{
			Node->childrenPageNums.push_back(siblingNode->childrenPageNums[i]);
			Node->children.push_back(siblingNode->children[i]);
			i++;
		}

		//delete the keys and children pointers in siblingNode
		i = 0;
		while(i < even_no)
		{
			siblingNode->keys.erase(siblingNode->keys.begin());
			siblingNode->childrenPageNums.erase(siblingNode->childrenPageNums.begin());
			siblingNode->children.erase(siblingNode->children.begin());
			i++;
		}
	}
	return SUCCESS;
}

/*
 * redistribute elements evenly between the leaf node and its sibling
 * assume there are more elements in siblingNode to spare
 */
template <typename KEY>
RC BTree<KEY>::redistribute_LeafNode(BTreeNode<KEY>* Node,BTreeNode<KEY>* siblingNode)
{
    int i = 0;
    int even_no = (siblingNode->keys.size() + Node->keys.size())/2;
	if(Node->Pos > siblingNode->Pos)
	{// left sibling

		// update the key value of parent
		Node->parent->keys[siblingNode->Pos] = siblingNode->keys[even_no];

		// move keys and RIDs from siblingNode to Node
		for(i = siblingNode->keys.size()-1; i > even_no; i--)
		{
		    Node->keys.insert(Node->keys.begin(),siblingNode->keys[i]);
		    Node->rids.insert(Node->rids.begin(),siblingNode->rids[i]);
		}

		//delete the keys and RIDs in siblingNode
		while(siblingNode->keys.size() > even_no)
		{
			siblingNode->keys.erase(siblingNode->keys.end());
			siblingNode->rids.erase(siblingNode->rids.end());
		}
	}
	else
	{// right sibling
		even_no = (siblingNode->keys.size()- Node->keys.size())/2;

		// update the key value of parent
		Node->parent->keys[Node->Pos] = siblingNode->keys[even_no-1];

		// move keys and RIDs from siblingNode to Node
		i = 0;
		while(i < even_no - 1)
		{
		    Node->keys.push_back(siblingNode->keys[i]);
		    Node->rids.push_back(siblingNode->rids[i]);
		    i++;
		}

		//delete the keys and RIDs in siblingNode
		i = 0;
		while(i < even_no)
		{
			siblingNode->keys.erase(siblingNode->keys.begin());
			siblingNode->rids.erase(siblingNode->rids.begin());
			i++;
		}
	}
	return SUCCESS;
}

/*
 *  merge a leaf node with its sibling, always merge right node into left node
 */
template <typename KEY>
RC BTree<KEY>::merge_LeafNode(BTreeNode<KEY>* leftNode,BTreeNode<KEY>* rightNode)
{
	// move keys and RIDs from right node to left node
	for( int i = 0; i < rightNode->keys.size(); i++ )
	{
		leftNode->keys.push_back(rightNode->keys[i]);
		leftNode->rids.push_back(rightNode->rids[i]);
	}

	// adjust the sibling pointers
	leftNode->right = rightNode->right;
	leftNode->rightPageNum = rightNode->rightPageNum;
	return SUCCESS;
}

/*
 *  merge a non-leaf node with its sibling, for non-leaf merge,
 *  we always merge into the left node
 */
template <typename KEY>
RC BTree<KEY>::merge_NLeafNode(BTreeNode<KEY>* leftNode,BTreeNode<KEY>* rightNode)
{
	int i = 0;
	//insert the entry of parent into the left node
	leftNode->keys.push_back(leftNode->parent->keys[leftNode->pos]);

	// move keys from right to left
    for( i = 0; i < rightNode->keys.size(); i++ )
    {
		leftNode->keys.push_back(rightNode->keys[i]);
	}

    // move childrenPageNum and children pointers from right to left
    for( i = 0; i < rightNode->childrenPageNums.size(); i++ )
    {
    	leftNode->childrenPageNums.push_back(rightNode->childrenPageNums[i]);
    	leftNode->children.push_back(rightNode->children[i]);
    }

	return SUCCESS;
}

/*
 *  delete the node recursively
 */
template <typename KEY>
RC BTree<KEY>::deleteNode(BTreeNode<KEY>* Node,int nodeLevel, const KEY key, unsigned& oldchildPos)
{
	int i = 0;
	if ( nodeLevel < this->_height)
	{// non-leaf node
		BTreeNode<KEY>* childNode = new BTreeNode<KEY>;
		// find the way to go
		for (i = 0; i < Node->keys.size(); i++)
		{
			if ( key < (Node->keys[i]) )
			{
				break;
			}
		}
		//TODO: get the child node
		this->_func_ReadNode(Node->childrenPageNums[i],NON_LEAF_NODE);
		deleteNode(childNode,nodeLevel+1, key, oldchildPos);
		if(oldchildPos == -1)
		{// usual case, child not deleted
			return SUCCESS;
		}
		else
		{// discard the child node
			Node->keys.erase(Node->keys.begin()+i);
			Node->childrenPageNum.erase(Node->childrenPageNums.begin()+i+1);
			Node->children.erase(Node->children.begin()+i+1);
		}

		// TODO: get the siblingNode of the currentNode
		int siblingPageNum = 0;
		NodeType siblingType = 0;
		BTreeNode<KEY>* siblingNode = new BTreeNode<KEY>;
		if( Node->pos < Node->parent->keys.size()-1 )
		{
			siblingPageNum = Node->parent->childrenPageNums[Node->pos+1];
		}
		else
		{
			siblingPageNum = Node->parent->childrenPageNums[Node->pos-1];
		}
		this->_func_ReadNode(siblingPageNum,NON_LEAF_NODE);


		if(siblingNode->keys.size() > this->_order)
		{// if the sibling node has extra entries
			redistribute_NLeafNode(Node,siblingNode);
			oldchildPos = -1;
			return SUCCESS;
		}
		else
		{// merge the current node with its sibling node
			merge_NLeafNode(Node,siblingNode);
			oldchildPos = siblingNode->pos;
			return SUCCESS;
		}

	}
	else
	{// leaf node
		if( Node->keys.size() -1 >= this->_order )
		{// usual case
			for(i = 0; i < Node->keys.size(); i++)
			{
				if(key == Node->keys[i])
				{
					Node->keys.erase(Node->keys.begin()+i);
					Node->rids.erase(Node->rids.begin()+i);
					break;
				}
			}
			if(i == Node->keys.size())
			{
				return RECORD_NOT_FOUND;
			}
			oldchildPos = -1;
			return SUCCESS;
		}

		// TODO: get the siblingNode of the currentNode
		int siblingPageNum = 0;
		NodeType siblingType = 0;
		BTreeNode<KEY>* siblingNode = new BTreeNode<KEY>;
		if( Node->pos < Node->parent->keys.size()-1 )
		{
			siblingPageNum = Node->parent->childrenPageNums[Node->pos+1];
		}
		else
		{
			siblingPageNum = Node->parent->childrenPageNums[Node->pos-1];
		}
		this->_func_ReadNode(siblingPageNum,LEAF_NODE);

        if(siblingNode->keys.size() > this->_order)
		{// if the sibling node has extra entries
        	redistribute_LeafNode(Node,siblingNode);
			oldchildPos = -1;
			return SUCCESS;
		}
		else
		{// merge the current node with its sibling node
			merge_LeafNode(Node,siblingNode);
			oldchildPos = siblingNode->pos;
			return SUCCESS;
		}
	}

	return SUCCESS;
}

/* ================== Protected Functions End ================== */

/* ================== Public Functions End ================== */

template <typename KEY>
RC BTree<KEY>::SearchEntry(const KEY key, BTreeNode<KEY> **leafNode, unsigned &pos)
{
	return SearchNode(this->_root, key, this->_height, leafNode, pos);
}

template <typename KEY>
RC BTree<KEY>::InsertEntry(const KEY key, const RID &rid)
{
	BTreeNode<KEY> *leafNode;
	unsigned pos;
	RC rc = SearchEntry(key, &leafNode, pos);
	if (DEBUG)
	{
		cout << endl << endl << endl;
		cout << "***************************************************************************" << endl;
		cout << "====== BTree<KEY>::InsertEntry - Found position [" << pos << "] on leaf node [" << leafNode->pageNum << "]." << endl;
		if (leafNode->parent && leafNode->parent->parent)
		{
			cout << "Parent's parent:" << endl;
			PrintNode(leafNode->parent->parent);
			cout << endl;
		}
		if (leafNode->parent)
		{
			cout << "Parent:" << endl;
			PrintNode(leafNode->parent);
			cout << endl;
		}
		if (leafNode->left)
		{
			cout << "Left:" << endl;
			PrintNode(leafNode->left);
			cout << endl;
		}
		cout << "Self:" << endl;
		PrintNode(leafNode);
		cout << endl;
		if (leafNode->right)
		{
			cout << "Right:" << endl;
			PrintNode(leafNode->right);
		}
		cout << "***************************************************************************";
		cout << endl << endl << endl;
	}

	if (rc == SUCCESS)
	{
		IX_PrintError(KEY_EXISTS);		// TODO: Extra Credit -- Overflow
		return KEY_EXISTS;
	}
	else
	{
		RC rc = Insert(key, rid, leafNode, pos);
		if (DEBUG)
		{
			cout << endl << endl << endl;
			cout << "***************************************************************************" << endl;
			cout << "====== BTree<KEY>::InsertEntry - Don insertion." << endl;
			if (leafNode->parent && leafNode->parent->parent)
			{
				cout << "Parent's parent:" << endl;
				PrintNode(leafNode->parent->parent);
				cout << endl;
			}
			if (leafNode->parent)
			{
				cout << "Parent:" << endl;
				PrintNode(leafNode->parent);
				cout << endl;
			}
			if (leafNode->left)
			{
				cout << "Left:" << endl;
				PrintNode(leafNode->left);
				cout << endl;
			}
			cout << "Self:" << endl;
			PrintNode(leafNode);
			cout << endl;
			if (leafNode->right)
			{
				cout << "Right:" << endl;
				PrintNode(leafNode->right);
			}
			cout << "***************************************************************************";
			cout << endl << endl << endl;
		}
		return rc;
	}
}

// TODO: Implementation!!!
template <typename KEY>
RC BTree<KEY>::DeleteTree()
{
	return SUCCESS;
}

template <typename KEY>
vector<BTreeNode<KEY>*> BTree<KEY>::GetUpdatedNodes() const
{
	return this->_updated_nodes;
}

template <typename KEY>
vector<BTreeNode<KEY>*> BTree<KEY>::GetDeletedNodes() const
{
	return this->_deleted_nodes;
}

template <typename KEY>
void BTree<KEY>::ClearPendingNodes()
{
	this->_updated_nodes.clear();
	this->_deleted_nodes.clear();
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
	if (!Exist(fileName))
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
	PF_FileHandle *handle = new PF_FileHandle;
	if (_pf_manager->OpenFile(IX_FILE_NAME(tableName, attributeName).c_str(), *handle) != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}

//	RM *rm = RM::Instance();
//	RM_ScanIterator rmsi;
//	string attr = "TYPE";
//    vector<string> attributes;
//    attributes.push_back(attr);
//    rm->scan(tableName, "COLUMN_NAME", CompOp(0), attributeName.c_str(), attributes, rmsi);
//
//    RID rid;
//    void *data_returned = malloc(100);
//	char *keyType;
//    while(rmsi.getNextTuple(rid, data_returned) != RM_EOF)
//    {
//    	unsigned len = 0;
//    	memcpy(&len, data_returned, 4);
//    	keyType = (char *)malloc(1);
//    	memcpy(keyType, (char *)data_returned + 4, 1);
//    	cout << "IX_Manager::OpenIndex - Opening index for ";
//    	cout << tableName << "." << attributeName << " [" << keyType << "]" << "." << endl;
//    }
//    if (keyType == NULL)
//    {
//    	IX_PrintError(ATTRIBUTE_NOT_FOUND);
//    	return ATTRIBUTE_NOT_FOUND;
//    }
//    free(data_returned);
//    rmsi.close();
	char *keyType = (char *)malloc(1);
	string i = "i";
	memcpy(keyType, i.c_str(), 1);

	return indexHandle.Open(handle, keyType);
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

/* ================== Protected Functions Begin ================== */

/**
 * Initializes meta-data of new index file.
 */
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

/* ================== Protected Functions End ================== */

/********************* IX_Manager End *********************/

/********************* IX_IndexHandle Begin *********************/

IX_IndexHandle::IX_IndexHandle()
{
	this->_pf_handle = NULL;
	this->_key_type = NULL;
	this->_free_page_num = 0;
	this->_int_index = NULL;
	this->_float_index = NULL;
}

IX_IndexHandle::~IX_IndexHandle()
{
	if (this->_key_type)
		free(this->_key_type);
	if (this->_int_index)
		delete this->_int_index;
	if (this->_float_index)
		delete this->_float_index;
}

/* ================== Protected Functions Begin ================== */

// TODO: consider separating this function from IX_IndexHandle class, and pass in a file handle instead -- can also simplify Functor
/**
 * Reads node of nodeType from index file with given page number.
 */
template <typename KEY>
BTreeNode<KEY>* IX_IndexHandle::ReadNode(const unsigned pageNum, const NodeType nodeType)
{
	const unsigned KEY_LENGTH = 4;
	const unsigned RID_LENGTH = 8;
	const unsigned PAGE_NUM_LENGTH = 4;

	void *page = malloc(PF_PAGE_SIZE);
	this->_pf_handle->ReadPage(pageNum, page);
	unsigned offset = 0;

	unsigned nodeNum = 0;
	memcpy(&nodeNum, page, 4);
	offset += 4;
	cout << "IX_IndexHandle::ReadNode - Reading " << nodeNum << " key(s) from page " << pageNum << " as type " << nodeType << "." << endl;

	BTreeNode<KEY> *node = new BTreeNode<KEY>;
	node->pageNum = pageNum;
	node->type = nodeType;
	node->parent = NULL;
	node->left = NULL;
	node->right = NULL;
	node->pos = 0;
	if (nodeType == NodeType(1))	// read leaf node
	{
		// read double direction links
		memcpy(&node->leftPageNum, (char *)page + offset, 4);
		offset += 4;
		memcpy(&node->rightPageNum, (char *)page + offset, 4);
		offset += 4;

		// read key and rid pairs
		for (unsigned index = 0; index < nodeNum; index++)
		{
			KEY key;
			RID rid;
			memcpy(&key, (char *)page + offset, KEY_LENGTH);
			offset += KEY_LENGTH;
			memcpy(&rid, (char *)page + offset, RID_LENGTH);
			offset += RID_LENGTH;

			node->keys.push_back(key);
			node->rids.push_back(rid);
		}
	}
	else	// read non-leaf node
	{
		// read leftmost child page number
		unsigned childPageNum = 0;
		memcpy(&childPageNum, (char *)page + offset, PAGE_NUM_LENGTH);
		offset += PAGE_NUM_LENGTH;
		node->childrenPageNums.push_back(childPageNum);

		// read keys and other children page numbers
		for (unsigned index = 0; index < nodeNum; index++)
		{
			KEY key;
			memcpy(&key, (char *)page + offset, KEY_LENGTH);
			offset += KEY_LENGTH;
			memcpy(&childPageNum, (char *)page + offset, PAGE_NUM_LENGTH);
			offset += PAGE_NUM_LENGTH;

			node->keys.push_back(key);
			node->childrenPageNums.push_back(childPageNum);
		}
	}

	free(page);
	return node;
}

/**
 * Writes node data to file, and update page number for the node.
 */
template <typename KEY>
void IX_IndexHandle::WriteNodes(const vector<BTreeNode<KEY>*> &nodes)
{
	const unsigned KEY_LENGTH = 4;
	const unsigned RID_LENGTH = 8;
	const unsigned PAGE_NUM_LENGTH = 4;

	for (unsigned index = 0; index < nodes.size(); index++)
	{
		BTreeNode<KEY> *node = nodes[index];
		void *page = malloc(PF_PAGE_SIZE);

		if (node->pageNum == -1) 	// new node
		{
			if (this->_free_page_num != 0)	// reuse free page
			{
				node->pageNum = this->_free_page_num;
				this->_pf_handle->ReadPage(this->_free_page_num, page);
				memcpy(&this->_free_page_num, page, 4);

				cout << "IX_IndexHandle::WriteNodes - Re-use free page " << node->pageNum << "; next free page is " << this->_free_page_num << "." << endl;
			}
			else	// new page
			{
				node->pageNum = this->_pf_handle->GetNumberOfPages();
			}

			if (node->parent)
				node->parent->childrenPageNums[node->pos] = node->pageNum;
			if (node->type == NodeType(1))
			{
				if (node->left)
					node->left->rightPageNum = node->pageNum;
				if (node->right)
					node->right->leftPageNum = node->pageNum;
			}
			cout << "IX_IndexHandle::WriteNodes - New page will be written on page " << node->pageNum << "." << endl;
		}

		// update page data
		unsigned offset = 0;
		unsigned size = node->keys.size();
		memcpy(page, &size, 4);
		offset += 4;

		if (node->type == NodeType(1))	// leaf node
		{
			memcpy((char *)page + offset, &node->leftPageNum, PAGE_NUM_LENGTH);
			offset += PAGE_NUM_LENGTH;
			memcpy((char *)page + offset, &node->rightPageNum, PAGE_NUM_LENGTH);
			offset += PAGE_NUM_LENGTH;

			for (unsigned index = 0; index < node->keys.size(); index++)
			{
				memcpy((char *)page + offset, &node->keys[index], KEY_LENGTH);
				offset += KEY_LENGTH;
				memcpy((char *)page + offset, &node->rids[index], RID_LENGTH);
				offset += RID_LENGTH;
			}
		}
		else	// non-leaf node
		{
			for (unsigned index = 0; index < node->keys.size(); index++)
			{
				memcpy((char *)page + offset, &node->childrenPageNums[index], PAGE_NUM_LENGTH);
				offset += PAGE_NUM_LENGTH;
				memcpy((char *)page + offset, &node->keys[index], KEY_LENGTH);
				offset += KEY_LENGTH;
			}
			if (node->childrenPageNums.size() > 1)
				memcpy((char *)page + offset, &node->childrenPageNums[node->childrenPageNums.size() - 1], PAGE_NUM_LENGTH);
		}

		this->_pf_handle->WritePage(node->pageNum, page);
		free(page);
		cout << "IX_IndexHandle::WriteNodes - Wrote one " << (node->type == NodeType(1) ? "leaf" : "non-leaf") << " node on page " << node->pageNum << "." << endl;
	}
}

template <typename KEY>
RC IX_IndexHandle::InitTree(BTree<KEY> **tree)
{
	// read meta-data
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

	// initialize tree
	if (level > 0)	// read root from file
	{
		cout << "IX_IndexHandle::InitTree - Reading the root [at page " << rootPageNum << "] for a tree of level " << level << "." << endl;
		NodeType rootNodeType = level == 1 ? NodeType(1) : NodeType(0);
		BTreeNode<KEY> *root = ReadNode<KEY>(rootPageNum, rootNodeType);
		*tree = new BTree<KEY>(DEFAULT_ORDER, root, level, this, &IX_IndexHandle::ReadNode<KEY>);
	}
	else	// create an empty tree
	{
		cout << "IX_IndexHandle::InitTree - Initializing a tree with empty root as a leaf node." << endl;
		*tree = new BTree<KEY>(DEFAULT_ORDER, this, &IX_IndexHandle::ReadNode<KEY>);
	}
	this->_free_page_num = freePageNum;

	return SUCCESS;
}

template <typename KEY>
RC IX_IndexHandle::InsertEntry(BTree<KEY> **tree, const KEY key, const RID &rid)
{
	if (*tree == NULL)
		this->InitTree(tree);

	return (*tree)->InsertEntry(key, rid);
}

/* ================== Protected Functions End ================== */

/* ================== Public Functions Begin ================== */

RC IX_IndexHandle::InsertEntry(void *key, const RID &rid)
{
	RC rc;
	if (strcmp(this->_key_type, typeid(int).name()) == 0)
	{
		const int intKey = *(int *)key;
		rc = InsertEntry(&this->_int_index, intKey, rid);
		this->WriteNodes(this->_int_index->GetUpdatedNodes());
		this->_int_index->ClearPendingNodes();
	}
	else if (strcmp(this->_key_type, typeid(float).name()) == 0)
	{
		const float floatKey = *(float *)key;
		rc = InsertEntry(&this->_float_index, floatKey, rid);
		this->WriteNodes(this->_float_index->GetUpdatedNodes());
		this->_int_index->ClearPendingNodes();
	}
	return rc;
}

RC IX_IndexHandle::Open(PF_FileHandle *handle, char *keyType)
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

	if (this->_int_index)
		delete this->_int_index;
	if (this->_float_index)
		delete this->_float_index;
	this->_pf_handle = NULL;
	this->_int_index = NULL;
	this->_float_index = NULL;
	return SUCCESS;
}

PF_FileHandle* IX_IndexHandle::GetFileHandle() const
{
	return this->_pf_handle;
}

/* ================== Public Functions End ================== */

/********************* IX_IndexHandle End *********************/

/********************* IX_IndexScan Start *********************/



/********************* IX_IndexScan End *********************/

void IX_PrintError(RC rc)
{
	string errMsg;

	switch (rc)
	{
	case INVALID_OPERATION:
		errMsg = "Invalid operation.";
		break;
	case ATTRIBUTE_NOT_FOUND:
		errMsg = "Cannot find the given attribute in the given table.";
		break;
	case KEY_EXISTS:
		errMsg = "Key exists.";
		break;
	}

	cout << "Encountered error: " << errMsg << endl;
}
