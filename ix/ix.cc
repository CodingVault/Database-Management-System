
#include <iostream>
#include <queue>
#include "ix.h"

#define IX_FILE_NAME(tableName, attrName) ("IX_" + tableName + "_" + attrName + ".idx")
#define DEBUG false

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
void PrintNodeContent(const BTreeNode<KEY> *node)
{
	cout << "{Page #: " << node->pageNum << ", ";
	cout << "Key(s) #: " << node->keys.size() << ", ";
	cout << "Parent: " << (node->parent == NULL ? -1 : node->parent->pageNum) << ", ";
	if (node->type == NodeType(1))	// leaf node
	{
		cout << "Left: " << node->leftPageNum << ", ";
		cout << "Right: " << node->rightPageNum << ", ";

		for (unsigned index = 0; index < node->keys.size(); index++)
			cout << "(" << index << ") <" << node->keys[index] << "> -> [" << node->rids[index].pageNum << ":" << node->rids[index].slotNum << "], ";
	}
	else	// non-leaf node
	{
		for (unsigned index = 0; index < node->keys.size(); index++)
			cout << "(" << node->childrenPageNums[index] << ") <" << node->keys[index] << "> ";
		if (node->childrenPageNums.size() > 1)
			cout << "(" << node->childrenPageNums[node->childrenPageNums.size() - 1] << ")";
	}
	cout << "} ";
}

template <typename KEY>
void PrintNode(const BTreeNode<KEY> *node)
{
	cout << "====================================================" << endl;
	cout << "Print node information as " << (node->type == NodeType(1) ? "leaf node." : "non-leaf node.") << endl;
	PrintNodeContent(node);
	cout << endl << "====================================================" << endl;
}

template <typename KEY>
void PrintTree(const BTree<KEY> *tree)
{
	cout << "====================================================" << endl;
	cout << "Print the tree:" << endl;

	unsigned level = 1;
	queue<BTreeNode<KEY>* > procNodes;
	procNodes.push(NULL);
	procNodes.push(tree->GetRoot());

	while (procNodes.size() > 1)
	{
		BTreeNode<KEY> *node = procNodes.front();
		if (node == NULL)
		{
			cout << endl << "LEVEL " << level++ << " ["<< procNodes.size() << " node(s)]: ";
			procNodes.pop();
			procNodes.push(NULL);
			node = procNodes.front();
		}

		PrintNodeContent(node);
		for (unsigned index = 0; index < node->children.size(); index++)
			if (node->children[index])
				procNodes.push(node->children[index]);
		procNodes.pop();
	}
	cout << endl << "====================================================" << endl;
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
	DeleteTree(this->_root);
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

	RC rc = SUCCESS;
	if (leafNode->keys.size() > this->_order * 2)	// need to split
	{
		BTreeNode<KEY> *newNode = new BTreeNode<KEY>;
		Split(this->_order, leafNode, newNode);
		this->_updated_nodes.push_back(newNode);

		// update new node -- the right one
		newNode->type = NodeType(1);
		newNode->keys = vector<KEY>(leafNode->keys.begin() + this->_order, leafNode->keys.end());
		newNode->rids = vector<RID>(leafNode->rids.begin() + this->_order, leafNode->rids.end());

		// update old node -- the left one
		leafNode->keys.resize(this->_order);
		leafNode->rids.resize(this->_order);

		// update parent
		rc = this->Insert(newNode);
	}

	return rc;
}

/**
 * Inserts split node to parent node; recursive to parent's ancestor nodes if necessary.
 */
template <typename KEY>
RC BTree<KEY>::Insert(BTreeNode<KEY> *rightNode)
{
	const KEY key = rightNode->left->keys[this->_order];

	if (rightNode->parent == NULL)		// reach root
	{
		InitRootNode(NodeType(0));
		this->_root->children.push_back(rightNode->left);
		this->_root->childrenPageNums.push_back(rightNode->left->pageNum);
		rightNode->parent = this->_root;
		rightNode->left->parent = this->_root;
		this->_height++;
	}
	BTreeNode<KEY> *parent = rightNode->parent;

	// update parent node
	typename vector<KEY>::iterator itKey = parent->keys.begin() + rightNode->pos - 1;
	parent->keys.insert(itKey, key);

	typename vector<BTreeNode<KEY>*>::iterator itChildren = parent->children.begin() + rightNode->pos;
	parent->children.insert(itChildren, rightNode);

	vector<int>::iterator itChildrenPageNums = parent->childrenPageNums.begin() + rightNode->pos;
	parent->childrenPageNums.insert(itChildrenPageNums, rightNode->pageNum);	// insert -1 in effect
	this->_updated_nodes.push_back(parent);

	RC rc = SUCCESS;
	if (parent->keys.size() > this->_order * 2)	// need to split
	{
		BTreeNode<KEY> *newNode = new BTreeNode<KEY>;
		Split(this->_order, parent, newNode);
		this->_updated_nodes.push_back(newNode);

		// update new node -- the right one, and its children's parent (pointer and page number)
		newNode->type = NodeType(0);
		newNode->keys = vector<KEY>(parent->keys.begin() + this->_order + 1, parent->keys.end());
		newNode->children.insert(newNode->children.begin(), parent->children.begin() + this->_order + 1, parent->children.end());
		newNode->childrenPageNums.insert(newNode->childrenPageNums.begin(),
				parent->childrenPageNums.begin() + this->_order + 1,
				parent->childrenPageNums.end());
		// NOTE: remember to update its children's parent and their position, but these changes do not need to update to file
		for (unsigned index = 0; index < newNode->children.size(); index++)
		{
			if (newNode->children[index])
			{
				newNode->children[index]->parent = newNode;
				newNode->children[index]->pos = index;
			}
		}

		// update old node -- the left one
		parent->keys.resize(this->_order);
		parent->children.resize(this->_order + 1);
		parent->childrenPageNums.resize(this->_order + 1);

		// update parent
		rc = this->Insert(newNode);
	}

	return rc;
}

/*
 * redistribute elements evenly between a non-leaf node and its sibling
 * assume there are more elements in the siblingNode to spare
 */
template <typename KEY>
void BTree<KEY>::redistribute_NLeafNode(BTreeNode<KEY>* Node,BTreeNode<KEY>* siblingNode)
{
    unsigned int i = 0;
    unsigned int even_no = 0;
	if( Node->pos > siblingNode->pos )
	{// left sibling
		even_no = (siblingNode->keys.size() + Node->keys.size())/2;
		// insert the key value of parent into Node
		Node->keys.insert(Node->keys.begin(),Node->parent->keys[siblingNode->pos]);
		// update the key value of parent
		Node->parent->keys[siblingNode->pos] = siblingNode->keys[even_no];

		// move keys from siblingNode to Node
		for( i = siblingNode->keys.size()-1; i > even_no; i-- )
		{
		    Node->keys.insert(Node->keys.begin(),siblingNode->keys[i]);
		}

		// move children pageNums and children pointers from siblingNode to Node
		for( i = siblingNode->children.size()-1; i > even_no; i-- )
		{
			Node->childrenPageNums.insert(Node->childrenPageNums.begin(),siblingNode->childrenPageNums[i]);
			Node->children.insert(Node->children.begin(),siblingNode->children[i]);
		}

		//delete the keys in siblingNode
		while( siblingNode->keys.size() > even_no )
		{
			siblingNode->keys.erase(siblingNode->keys.end());
		}
		//delete the childrenPageNums and children pointers in siblingNode
		while( siblingNode->childrenPageNums.size() > even_no+1 )
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
		Node->parent->keys[Node->pos] = siblingNode->keys[even_no-1];

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

		//delete the keys and children pointers of siblingNode
		i = 0;
		while(i < even_no)
		{
			siblingNode->keys.erase(siblingNode->keys.begin());
			siblingNode->childrenPageNums.erase(siblingNode->childrenPageNums.begin());
			siblingNode->children.erase(siblingNode->children.begin());
			i++;
		}
	}
}

/*
 * redistribute elements evenly between the leaf node and its sibling
 * assume there are more elements in siblingNode to spare
 */
template <typename KEY>
void BTree<KEY>::redistribute_LeafNode( BTreeNode<KEY>* Node, BTreeNode<KEY>* siblingNode )
{
    unsigned int i = 0;
    unsigned int even_no = 0;
	if( Node->pos > siblingNode->pos )
	{// left sibling
		even_no = (siblingNode->keys.size() + Node->keys.size())/2;
		// update the key value of parent
		Node->parent->keys[siblingNode->pos] = siblingNode->keys[even_no];

		// move keys and RIDs from siblingNode to Node
		for(i = siblingNode->keys.size()-1; i >= even_no; i--)
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
		Node->parent->keys[Node->pos] = siblingNode->keys[even_no-1];

		// move keys and RIDs from siblingNode to Node
		i = 0;
		while(i < even_no )
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
}

/*
 *  merge a leaf node with its sibling, always merge right node into left node
 */
template <typename KEY>
void BTree<KEY>::merge_LeafNode(BTreeNode<KEY>* leftNode,BTreeNode<KEY>* rightNode)
{
	// move keys and RIDs from right node to left node
	for( unsigned i = 0; i < rightNode->keys.size(); i++ )
	{
		leftNode->keys.push_back(rightNode->keys[i]);
		leftNode->rids.push_back(rightNode->rids[i]);
	}

	// adjust the sibling pointers
	leftNode->right = rightNode->right;
	leftNode->rightPageNum = rightNode->rightPageNum;

	//update the parent
	leftNode->parent->keys[leftNode->pos] = rightNode->keys[ rightNode->keys.size() - 1 ];
	leftNode->parent->keys[rightNode->pos] = rightNode->keys[ rightNode->keys.size() - 1 ];
}

/*
 *  merge a non-leaf node with its sibling
 *  we always merge into the left node
 */
template <typename KEY>
void BTree<KEY>::merge_NLeafNode(BTreeNode<KEY>* leftNode,BTreeNode<KEY>* rightNode)
{
	unsigned int i = 0;
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

    //update the parent
    leftNode->parent->keys[leftNode->pos] = rightNode->keys[ rightNode->keys.size() - 1 ];
    leftNode->parent->keys[rightNode->pos] = rightNode->keys[ rightNode->keys.size() - 1 ];
}

/*
 *  delete the non-leaf node recursively
 */
template <typename KEY>
RC BTree<KEY>::delete_NLeafNode(BTreeNode<KEY>* Node,unsigned nodeLevel, const KEY key, const RID &rid,int& oldchildPos)
{
	cout<<"delete in level "<<nodeLevel<<endl;
	RC rc = 0;
	unsigned int i = 0;
	BTreeNode<KEY>* childNode = NULL;
	unsigned keyNum = 0;
	// find the way to go
	for ( i = 0; i < Node->keys.size(); i++ )
	{
		if ( key < Node->keys[i] )
		{
			break;
		}
	}

	if( nodeLevel + 1 < this->_height )
	{// child node is a non-leaf node
		if(Node->children[i] == NULL)
		{// if the child is not in the memory
			childNode = this->_func_ReadNode( Node->childrenPageNums[i],NON_LEAF_NODE );
		}
	}
	else
	{//child node is a leaf node
		if(Node->children[i] == NULL)
		{
			childNode = this->_func_ReadNode( Node->childrenPageNums[i],LEAF_NODE );
		}
	}

	if( childNode != NULL )
	{
		childNode->parent = Node;
		childNode->pos = i;
		Node->children[i] = childNode;
	}

	if( nodeLevel + 1< this->_height )
	{// child node is a non-leaf node
		cout<<"go down to non-leaf level "<< nodeLevel+1<<endl;
		rc = delete_NLeafNode( Node->children[i], nodeLevel+1, key, rid, oldchildPos );
		if( rc != SUCCESS )
		{
			return rc;
		}
	}
	else
	{// child node is a leaf node
		cout<<"go down to leaf level "<< nodeLevel+1<<endl;
		rc = delete_LeafNode( Node->children[i], key, rid, oldchildPos );
		if( rc != SUCCESS )
		{
			return rc;
		}
	}

	if(oldchildPos == -1)
	{// usual case, child not deleted
		return SUCCESS;
	}

	if( nodeLevel == 1 )
	{// the current node is the root
		keyNum = Node->keys.size();
		if( keyNum > 1 )
		{// if the root node has entries to spare
			if( oldchildPos > 0 )
			{// the child node deleted is not at leftmost
				Node->keys.erase(Node->keys.begin() +  oldchildPos - 1);
			}
			else
			{// the child node deleted is at leftmost
				Node->keys.erase(Node->keys.begin() +  oldchildPos );
			}

			//remove the childrenPageNum and children pointer
			Node->childrenPageNums.erase(Node->childrenPageNums.begin() + oldchildPos);
			Node->children.erase(Node->children.begin() + oldchildPos);
			this->_updated_nodes.push_back(Node);
			return SUCCESS;
		}
		else
		{// if the root node has only one entry left, the height of the tree will decrease by 1
			i = 1 - oldchildPos;
			//get the remained child of the old root as the new root
            if( nodeLevel < this->_height )
		    {// child node is a non-leaf node
            	if( Node->children[i] == NULL )
            	{
            		childNode = this->_func_ReadNode( Node->childrenPageNums[i],NON_LEAF_NODE );
            	}
			}
			else
			{//child node is a leaf node
				if( Node->children[i] == NULL )
				{
					childNode = this->_func_ReadNode( Node->childrenPageNums[i],LEAF_NODE );
				}
			}

            //set the new root
            Node->children[0] = childNode;
			this->_root = Node->children[0];
			Node->children[0]->parent = NULL;

            //TODO: update meta data, since new root is created
			this->_updated_nodes.push_back(this->_root);// update the new root
			this->_deleted_nodes.push_back(Node);// delete the old root
			this->_height--;

			Node = NULL;
			return SUCCESS;
		}
	}// root node

	/**************      non-root node   ********************/
	if( oldchildPos > 0 )
	{// the child node deleted is not at leftmost
		Node->keys.erase( Node->keys.begin() +  oldchildPos - 1 );
	}
	else
	{// the child node deleted is at leftmost
		Node->keys.erase( Node->keys.begin() +  oldchildPos );
	}
	Node->childrenPageNums.erase(Node->childrenPageNums.begin() + oldchildPos);
	Node->children.erase(Node->children.begin() + oldchildPos);

	if( Node->keys.size() >= this->_order )
	{// usual case, discard the child node
		this->_updated_nodes.push_back(Node);
		return SUCCESS;
	}

	// the current node has not extra items, get its sibling

	int siblingPos = 0;
	int siblingPageNum = 0;
	BTreeNode<KEY>* siblingNode = NULL;
	if( Node->pos < Node->parent->children.size()-1 )
	{// right sibling, if the node is not at rightmost
		siblingPos = Node->pos + 1;
		siblingPageNum = Node->parent->childrenPageNums[siblingPos];
	}
	else
	{// left sibling, if the node is at rightmost
		siblingPos = Node->pos - 1;
		siblingPageNum = Node->parent->childrenPageNums[siblingPos];
	}

	if(Node->parent->children[siblingPos] == NULL)
	{// if the sibling node is not in the memory
		siblingNode = this->_func_ReadNode(siblingPageNum,NON_LEAF_NODE);
		siblingNode->parent = Node->parent;
		siblingNode->pos = siblingPos;
		Node->parent->children[siblingPos] = siblingNode;
	}
	else
	{
		siblingNode = Node->parent->children[siblingPos];
	}

	if(siblingNode->keys.size()-1 >= this->_order)
	{// if the sibling node has extra entries
		redistribute_NLeafNode(Node,siblingNode);
		oldchildPos = -1;
		this->_updated_nodes.push_back(Node);
		this->_updated_nodes.push_back(siblingNode);
		this->_updated_nodes.push_back(Node->parent);
		return SUCCESS;
	}
	else
	{// merge the current node with its sibling node
		if( Node->pos < siblingNode->pos)
		{// right sibling
			merge_NLeafNode(Node,siblingNode);
			this->_deleted_nodes.push_back(siblingNode);
			this->_updated_nodes.push_back(Node);
			this->_updated_nodes.push_back(Node->parent);
			oldchildPos = siblingNode->pos;
		}
		else
		{
			merge_NLeafNode(siblingNode,Node);
			this->_deleted_nodes.push_back(Node);
			this->_updated_nodes.push_back(siblingNode);
			this->_updated_nodes.push_back(Node->parent);
			oldchildPos = Node->pos;
		}
		return SUCCESS;
	}
}

template <typename KEY>
RC BTree<KEY>::delete_LeafNode(BTreeNode<KEY>* Node, const KEY key,const RID &rid, int& oldchildPos)
{
	cout<<"delete in leaf node"<<endl;
	unsigned i = 0;
	unsigned keysNum = 0;
	if(this->_height == 1)
	{// the root is a leaf
		keysNum = Node->keys.size();
		if( keysNum > 1 )
		{// the root has more than one entry
			for( i = 0; i < keysNum; i++ )
			{
				if( key == Node->keys[i] && (Node->rids[i]).pageNum ==  rid.pageNum
						&& Node->rids[i].slotNum == rid.slotNum )
				{// find the entry
					Node->keys.erase(Node->keys.begin()+i);
					Node->rids.erase(Node->rids.begin()+i);
					break;
				}
			}

			if(i == keysNum )
			{
				return RECORD_NOT_FOUND;
			}
			else
			{
				oldchildPos = -1;
				this->_updated_nodes.push_back(Node);
				return SUCCESS;
			}

		}
		else if(keysNum == 1)
		{// the tree becomes empty
			if( key == Node->keys[0] && Node->rids[0].pageNum ==  rid.pageNum
									&& Node->rids[0].slotNum == rid.slotNum )
			{
				this->_root = NULL;
				this->_deleted_nodes.push_back(Node);
				return SUCCESS;
			}
			else
			{
				return RECORD_NOT_FOUND;
			}
		}
		else
		{// empty tree
			return RECORD_NOT_FOUND;
		}

	}

	keysNum = Node->keys.size();
	for( i = 0; i < keysNum; i++ )
	{
		if( key == Node->keys[i] && Node->rids[i].pageNum ==  rid.pageNum
				&& Node->rids[i].slotNum == rid.slotNum )
		{
			Node->keys.erase(Node->keys.begin()+i);
			Node->rids.erase(Node->rids.begin()+i);
			break;
		}
	}
    if( i == keysNum )
	{
    	return RECORD_NOT_FOUND;
	}

	if( keysNum -1 >= this->_order )
	{// usual case, delete the child node
		cout<<"delete the key normally!"<<endl;
		oldchildPos = -1;
		this->_updated_nodes.push_back(Node);
		return SUCCESS;
	}

    cout<<"can't delete the key normally!"<<endl;
	// the node hasn't enough items to spare
	//get a sibling node
	unsigned siblingPos = 0;
    int siblingPageNum = 0;
	BTreeNode<KEY>* siblingNode = NULL;
	cout<<"the number of children is "<<Node->parent->children.size()<<endl;
	if( Node->pos < Node->parent->children.size()-1 )
	{// get the right sibling, if the node is not at rightmost
		cout<<"get the right sibling"<<endl;
		siblingPos = Node->pos + 1;
		siblingPageNum = Node->parent->childrenPageNums[siblingPos];
	}
	else
	{// get the left sibling, if the node is at rightmost
		cout<<"get the left sibling in position "<<(Node->pos - 1)<<endl;
		siblingPos = Node->pos - 1;
		siblingPageNum = Node->parent->childrenPageNums[siblingPos];
	}
	if( Node->parent->children[siblingPos] == NULL)
	{// if the sibling is not in memory, fetch it from disk
		cout<<"fetch the sibling from disk"<<endl;
		this->_func_ReadNode(siblingPageNum,LEAF_NODE);
		siblingNode->parent = Node->parent;
		siblingNode->pos = siblingPos;
		Node->parent->children[siblingPos] = siblingNode;
	}

	if( Node->pos < Node->parent->children.size()-1 )
	{// the node is not at rightmost,right sibling
		Node->right = siblingNode;
		siblingNode->left = Node;
	}
	else
	{// the node is at rightmost,left sibling
		Node->left = siblingNode;
		siblingNode->right = Node;
	}

    if(siblingNode->keys.size() > this->_order)
	{// if the sibling node has extra entries
    	cout<<"delete the key , need to redistribute!"<<endl;
	    redistribute_LeafNode(Node,siblingNode);
	    oldchildPos = -1;
		this->_updated_nodes.push_back(Node);
		this->_updated_nodes.push_back(siblingNode);
		this->_updated_nodes.push_back(Node->parent);
		return SUCCESS;
	}
	else
	{// if the sibling node hasn't extra entries, merge the current node with its sibling node
		if( Node->pos < siblingNode->pos)
		{// right sibling
			cout<<"delete the key , need to merge two leaf nodes!"<<endl;
			merge_LeafNode(Node,siblingNode);
			this->_deleted_nodes.push_back(siblingNode);
			this->_updated_nodes.push_back(Node);
			this->_updated_nodes.push_back(Node->parent);
			oldchildPos = siblingNode->pos;
		}
		else
		{// left sibling
			merge_LeafNode(siblingNode,Node);
			this->_deleted_nodes.push_back(Node);
			this->_updated_nodes.push_back(siblingNode);
			this->_updated_nodes.push_back(Node->parent);
			oldchildPos = Node->pos;
		}
		return SUCCESS;
	}

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
		cout << "BTree<KEY>::InsertEntry - Found position [" << pos << "] on leaf node [" << leafNode->pageNum << "]." << endl;
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
			cout << "BTree<KEY>::InsertEntry - Need to update " << this->_updated_nodes.size() << " node(s)." << endl;
			cout << "***************************************************************************" << endl;
			cout << "BTree<KEY>::InsertEntry - Don insertion." << endl;
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

template <typename KEY>
RC BTree<KEY>::DeleteEntry(const KEY key, const RID &rid)
{
	int oldchildPos = -1;
	cout<<"the height of the tree is: "<<this->_height<<endl;
	if(this->_root->type == LEAF_NODE)
	{
		return delete_LeafNode(this->_root,key,rid,oldchildPos);
	}
	else
	{
		return delete_NLeafNode(this->_root,1,key,rid,oldchildPos);
	}
}

template <typename KEY>
RC BTree<KEY>::DeleteTree(BTreeNode<KEY> *Node)
{
	if(Node == NULL)
	{
		return SUCCESS;
	}
	if(Node->type == LEAF_NODE)
	{
		delete Node;
	}
	else
	{
		for(unsigned int i = 0; i < Node->children.size(); i++ )
		{
			if( Node->children[i]!=NULL )
			{
				DeleteTree(Node->children[i]);
			}

		}
		delete Node;
	}

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

template <typename KEY>
BTreeNode<KEY>* BTree<KEY>::GetRoot() const
{
	return this->_root;
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
	//char *keyType = (char *)malloc(1);
	//string i = "i";
	//memcpy(keyType, i.c_str(), 1);
	//TODO: how to determine the type of the key
	AttrType keyType = TypeInt;

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
	this->_key_type = TypeInt;
	this->_free_page_num = 0;
	this->_int_index = NULL;
	this->_float_index = NULL;
}

IX_IndexHandle::~IX_IndexHandle()
{
	//if (this->_key_type)
	//	free(this->_key_type);
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
			node->children.push_back(NULL);
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
	cout<<typeid(int).name()<<endl;
	if ( _key_type == TypeInt )
	{

		const int intKey = *(int *)key;
		rc = InsertEntry(&this->_int_index, intKey, rid);
		this->WriteNodes(this->_int_index->GetUpdatedNodes());
		this->_int_index->ClearPendingNodes();
		if (DEBUG)
		{
			cout << "IX_IndexHandle::InsertEntry - Print int tree:" << endl;
			PrintTree(this->_int_index);
		}
	}
	else if ( _key_type == TypeReal )
	{
		const float floatKey = *(float *)key;
		rc = InsertEntry(&this->_float_index, floatKey, rid);
		this->WriteNodes(this->_float_index->GetUpdatedNodes());
		this->_int_index->ClearPendingNodes();
	}
	return rc;
}

RC IX_IndexHandle::DeleteEntry(void *key, const RID &rid)
{
	RC rc;
    if ( _key_type == TypeInt )
	{
			const int intKey = *(int *)key;
			rc = this->_int_index->DeleteEntry(intKey,rid);
			this->WriteNodes(this->_int_index->GetUpdatedNodes());
			this->_int_index->ClearPendingNodes();
	}
	else if ( _key_type == TypeReal )
	{
		const float floatKey = *(float *)key;
		rc = this->_float_index->DeleteEntry(floatKey,rid);
		this->WriteNodes(this->_float_index->GetUpdatedNodes());
		this->_float_index->ClearPendingNodes();
	}
    return rc;

}

RC IX_IndexHandle::Open(PF_FileHandle *handle, AttrType keyType)
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

AttrType IX_IndexHandle::GetKeyType() const
{
	return this->_key_type;
}

/* ================== Public Functions End ================== */

/********************* IX_IndexHandle End *********************/

/********************* IX_IndexScan Start *********************/

IX_IndexScan::IX_IndexScan()
{
}

IX_IndexScan::~IX_IndexScan()
{
}

/*
 * Initialize index scan
 */
template <typename KEY>
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
	      CompOp      compOp,
	      void        *value)
{
	BTreeNode<KEY>* Node = NULL;
	Functor<IX_IndexHandle, KEY> _func_ReadNode(indexHandle);
	AttrType keyType = indexHandle.GetKeyType();
	if( keyType == TypeReal )
	{
		memcpy(keyValue,value,4);
		this->floatNode = new BTreeNode<float>;
	}
	else if( keyType == TypeInt )
	{
		memcpy(keyValue,value,4);
		this->intNode = new BTreeNode<int>;
	}

	this->type = keyType;
	this->currentIndex = -1; // set the current index
	this->compOp = compOp;
	int height = 0;
	unsigned pageNum = 0;


	// fetch meta data
	char memory_page[PF_PAGE_SIZE] = "\0";
	PF_FileHandle* filehandle =  indexHandle.GetFileHandle();
	if(filehandle->ReadPage(0,memory_page))
	{
		return FILE_OP_ERROR;
	}
	pageNum = *(unsigned int*)( memory_page ); // the page number of the root
	height = *(int*)( memory_page + 4 );

	for( int i = 0; i < height - 1; i++ )
	{
		Node = _func_ReadNode(pageNum,NON_LEAF_NODE);
		pageNum = Node->childrenPageNums[0];
	}
	//fetch the leaf page
	Node = _func_ReadNode(pageNum,LEAF_NODE);

	if(  keyType == TypeReal )
	{
		memcpy(keyValue,value,4);
		this->floatNode = Node;
	}
	else if( keyType == TypeInt )
	{
		memcpy(keyValue,value,4);
		this->intNode = Node;
	}

	Node = NULL;

	return SUCCESS;
}

RC IX_IndexScan::GetNextEntry(RID &rid) // Get next matching entry
{
	return SUCCESS;
}

/*
 * get next matching entry
 */
template <typename KEY>
RC IX_IndexScan::get_next_entry(RID &rid)
{
	char currentValue[PF_PAGE_SIZE] = {"\0"};
	BTreeNode<KEY>* leafNode = NULL;
	Functor<IX_IndexHandle, KEY> _func_ReadNode(this->indexHandle);
    if(this->type == TypeInt)
    {
    	leafNode = this->intNode;
    }
    else
    {
    	leafNode = this->floatNode;
    }
	while(this->currentIndex + 1 < leafNode->keys.size())
	{
		currentIndex++;
		memcpy(currentValue,leafNode->keys[currentIndex],4);
		if(compare(currentValue, compOp, keyValue, this->type))
		{
			rid.pageNum = leafNode->rids[currentIndex].pageNum;
			rid.slotNum = leafNode->rids[currentIndex].slotNum;
			return SUCCESS;
		}
	}

	if( leafNode->rightPageNum == -1 )
	{// reach the rightmost page
		delete leafNode;
		return RECORD_NOT_FOUND;
	}

	// fetch the new leaf page
	delete leafNode;
    leafNode = _func_ReadNode.ReadNode(leafNode->rightPageNum,LEAF_NODE);
    currentIndex = 0;
	rid.pageNum = leafNode->rids[currentIndex].pageNum;
	rid.slotNum = leafNode->rids[currentIndex].slotNum;
	return SUCCESS;
}

/*
 *  Terminate index scan
 */
RC IX_IndexScan::CloseScan()
{
	this->floatNode = NULL;
	this->intNode = NULL;
	return SUCCESS;
}

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
