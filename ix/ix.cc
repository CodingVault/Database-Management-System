
#include <iostream>
#include <queue>
#include "ix.h"

#define IX_FILE_NAME(tableName, attrName) ("IX_" + tableName + "_" + attrName + ".idx")
#define DEBUG false

IX_Manager* IX_Manager::_ix_manager = 0;
PF_Manager* IX_Manager::_pf_manager = 0;

/**************************************/

RC GetAttrType(const string tableName, const string attributeName, AttrType &attrType)
{
	const string condAttr = "COLUMN_NAME";
	const string projAttr = "TYPE";

	RM *rm = RM::Instance();
	RM_ScanIterator rmsi;
    vector<Attribute> attributes;
    RC rc = rm->getAttributes(tableName, attributes);
    if (rc != SUCCESS)
    	return rc;

    for (unsigned index = 0; index < attributes.size(); index++)
    {
    	if (strcmp(attributeName.c_str(), attributes[index].name.c_str()) == 0)
    	{
    		attrType = attributes[index].type;
    		return SUCCESS;
    	}
    }

    IX_PrintError(ATTRIBUTE_NOT_FOUND);
    return ATTRIBUTE_NOT_FOUND;
}

RC WriteMetadata(PF_FileHandle &handle, const unsigned rootPageNum,
		const unsigned height, const unsigned freepageNum, const AttrType attrType)
{
	void *page = malloc(PF_PAGE_SIZE);
	memcpy(page, &rootPageNum, 4);
	memcpy((char *)page + 4, &height, 4);
	memcpy((char *)page + 8, &freepageNum, 4);
	memcpy((char *)page + 12, &attrType, 4);
	RC rc = handle.WritePage(0, page);
	free(page);

	if (rc != SUCCESS)
	{
		IX_PrintError(FILE_OP_ERROR);
		return FILE_OP_ERROR;
	}
	return SUCCESS;
}

template <typename KEY>
RC ExistInVector(const vector<KEY>& keys, const KEY& key )
{
	for(unsigned i = 0; i < keys.size(); i++)
	{
		if( key == keys[i])
		{
			return -1;
		}
	}
	return 0;
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
BTree<KEY>::BTree(const unsigned order, BTreeNode<KEY> *root, const unsigned height,
		IX_IndexHandle *ixHandle, BTreeNode<KEY>* (IX_IndexHandle::*func)(const unsigned, const NodeType))
		: _root(root), _order(order), _height(height), _func_ReadNode(ixHandle, func)
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

				if (DEBUG)
					cout << "BTree<KEY>::SearchNode - Found key at position [" << pos << "]." << endl;
				return SUCCESS;
			}
			else if (node->keys[index] > key)
			{
				pos = index;	// position for insertion
				return ENTRY_NOT_FOUND;
			}
		}
		pos = node->keys.size();	// position for insertion
		return ENTRY_NOT_FOUND;
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
void BTree<KEY>::RedistributeNLeafNode(BTreeNode<KEY>* Node,BTreeNode<KEY>* siblingNode)
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
			if( siblingNode->children[i] != NULL )
			{
			    siblingNode->children[i]->parent = Node;
			}
		}

		for( i = 0; i < Node->childrenPageNums.size(); i++)
		{
		    if( Node->children[i] != NULL )
		    {
		    	Node->children[i]->pos = i;
		    }
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
		//cout<<"redistribute with left sibling and the current node has "<<Node->keys.size()<<" keys"<<endl;
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
			if( siblingNode->children[i] != NULL)
			{
				siblingNode->children[i]->parent = Node;
			}
			i++;
		}

		for( i = 0; i < Node->childrenPageNums.size(); i++)
		{
			if( Node->children[i] != NULL )
			{
				Node->children[i]->pos = i;
			}
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
		//cout<<"redistribute with right sibling and the current node has "<<Node->keys.size()<<" keys"<<endl;
	}
}

/*
 * redistribute elements evenly between the leaf node and its sibling
 * assume there are more elements in siblingNode to spare
 */
template <typename KEY>
void BTree<KEY>::RedistributeLeafNode( BTreeNode<KEY>* Node, BTreeNode<KEY>* siblingNode )
{
	//cout<<"======redistribute begins===="<<endl;
    unsigned int i = 0;
    unsigned int even_no = 0;
	if( Node->pos > siblingNode->pos )
	{// left sibling
		//cout<<"the current node has "<<Node->keys.size()<<" items and its left sibling has"<<siblingNode->keys.size()<<endl;
		even_no = (siblingNode->keys.size() + Node->keys.size())/2;
		// update the key value of parent
		Node->parent->keys[siblingNode->pos] = siblingNode->keys[even_no];
        //cout<<"update the separate key in parent"<<endl;
		// move keys and RIDs from siblingNode to Node
		for(i = siblingNode->keys.size()-1; i >= even_no; i--)
		{
		    Node->keys.insert(Node->keys.begin(),siblingNode->keys[i]);
		    Node->rids.insert(Node->rids.begin(),siblingNode->rids[i]);
		}
		//cout<<"move the items of sibling node into the current node"<<endl;
		//delete the keys and RIDs in siblingNode
		while(siblingNode->keys.size() > even_no)
		{
			siblingNode->keys.erase(siblingNode->keys.end()-1);
			siblingNode->rids.erase(siblingNode->rids.end()-1);
			//cout<<"delete the items of sibling node"<<endl;
		}

	}
	else
	{// right sibling
		//cout<<"redistribute with right sibling"<<endl;
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
 *  merge a node with its sibling
 *  we always merge into the left node
 */
template <typename KEY>
void BTree<KEY>::MergeNode(BTreeNode<KEY>* leftNode,BTreeNode<KEY>* rightNode)
{
	unsigned int i = 0;
	if( leftNode->type == NON_LEAF_NODE)
	{
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
    	    leftNode->children.push_back( rightNode->children[i] );
    	    if( rightNode->children[i]!=NULL )
    	    {
    		    rightNode->children[i]->parent = leftNode;
    	    }
         }
         for( i = 0; i < leftNode->childrenPageNums.size(); i++)
         {
         	if( leftNode->children[i] != NULL )
    	    {
    		    leftNode->children[i]->pos = i;
    	    }
         }
	}
	else
	{
		// move keys and RIDs from right node to left node
		for( i = 0; i < rightNode->keys.size(); i++ )
		{
			leftNode->keys.push_back(rightNode->keys[i]);
			leftNode->rids.push_back(rightNode->rids[i]);
		}

		// adjust the sibling pointers
		leftNode->right = rightNode->right;
		leftNode->rightPageNum = rightNode->rightPageNum;
	}

    //update the parent
    leftNode->parent->keys[leftNode->pos] = leftNode->parent->keys[rightNode->pos];
}


/*
 *  delete the non-leaf node recursively
 */
template <typename KEY>
RC BTree<KEY>::DeleteNLeafNode(BTreeNode<KEY>* Node,unsigned nodeLevel, const KEY key, const RID &rid,int& deletedChildPos)
{
	unsigned int i = 0;
	RC rc = 0;
	unsigned keyNum = 0;

	/*********************************************************************************/
	//find the children to go down
	for ( i = 0; i < Node->children.size(); i++ )
	{
		if ( (i < Node->children.size()-1&& key < Node->keys[i]) || i == Node->children.size()-1 )
		{
			if( nodeLevel + 1 < this->_height )
			{// child node is a non-leaf node
				if(Node->children[i] == NULL)
				{// if the child is not in the memory
					Node->children[i] = this->_func_ReadNode( Node->childrenPageNums[i],NON_LEAF_NODE );
				}
			}
			else
			{//child node is a leaf node
				if(Node->children[i] == NULL)
				{
					Node->children[i] = this->_func_ReadNode( Node->childrenPageNums[i],LEAF_NODE );
				}
			}

			Node->children[i]->parent = Node;
			Node->children[i]->pos = i;

			if( nodeLevel + 1< this->_height )
			{// child node is a non-leaf node
				//cout<<"go down along the position "<<i<<" to non-leaf level "<< nodeLevel+1<<endl;
				rc = DeleteNLeafNode( Node->children[i], nodeLevel+1, key, rid, deletedChildPos );
				if( rc != SUCCESS )
				{
					return rc;
				}
			}
			else
			{// child node is a leaf node
				//cout<<"go down along the position "<<i<<" to leaf level "<< nodeLevel+1<<endl;
				rc = DeleteLeafNode( Node->children[i], key, rid, deletedChildPos );
				if( rc != SUCCESS )
				{
					return rc;
				}
			}
			break;
		}
	}

	/**************************************************************************************/
    // case 1 : the child is not deleted
	if(deletedChildPos == -1)
	{// usual case, child not deleted
		//cout<<"return to level "<<nodeLevel<<endl;
		//cout<<" usual case: child node was not deleted and the node has "<<Node->keys.size()<<" items"<<endl;
		return SUCCESS;
	}

	/*************************************************************************************/
	// case 2 : the current node is the root
	if( nodeLevel == 1 )
	{
        //cout<<"return to level "<<nodeLevel<<endl;
		keyNum = Node->keys.size();
		if( keyNum > 1 )
		{// if the root node has entries to spare
			if( deletedChildPos > 0 )
			{// the child node deleted is not at leftmost
				Node->keys.erase(Node->keys.begin() +  deletedChildPos - 1);
			}
			else
			{// the child node deleted is at leftmost
				Node->keys.erase(Node->keys.begin() +  deletedChildPos );
			}

			if( Node->type == NON_LEAF_NODE)
			{
				//remove the childrenPageNum and children pointer
			    Node->childrenPageNums.erase(Node->childrenPageNums.begin() + deletedChildPos);
				Node->children.erase(Node->children.begin() + deletedChildPos);
				// update the position of the children of the current node
				for( i = 0; i < Node->children.size(); i++ )
				{
					if( Node->children[i] != NULL )
					{
						Node->children[i]->pos = i;
						//cout<<Node->children[i]->pos<<" || ";
					}
				}
			}

			this->_updated_nodes.push_back(Node);
			//cout<<"delete in root at position "<<oldchildPos<<endl;
			//cout<<"the root has "<<Node->children.size()<<" children right now"<<endl;
			return SUCCESS;
		}
		else
		{// if the root node has only one entry left, the height of the tree will decrease by 1
			i = 1 - deletedChildPos;
			//get the remained child of the old root as the new root
            if( nodeLevel < this->_height )
		    {// child node is a non-leaf node
            	if( Node->children[i] == NULL )
            	{
            		Node->children[i] = this->_func_ReadNode( Node->childrenPageNums[i],NON_LEAF_NODE );
            	}
			}
			else
			{// child node is a leaf node
				if( Node->children[i] == NULL )
				{
					Node->children[i] = this->_func_ReadNode( Node->childrenPageNums[i],LEAF_NODE );
				}
			}

            //set the new root
			this->_root = Node->children[i];

			if( Node->type == NON_LEAF_NODE)
			{
				Node->children[i]->parent = NULL;
			}

			this->_updated_nodes.push_back(this->_root);// update the new root
			if(Node->pageNum > 0 && !ExistInVector( _deleted_pagenums, (unsigned)Node->pageNum ))
			{
				this->_deleted_pagenums.push_back(Node->pageNum);// delete the old root
				delete Node;
				Node = NULL;
			}
			//cout<<"Now there are "<<this->_deleted_pagenums.size()<<" free pages in the B Tree!"<<endl;
			this->_height--;
			//cout<<"the height of the tree decreases by 1 and the new root has "<<this->_root->children.size()<<" children"<<endl;
			return SUCCESS;
		}
	}

	/******************************************************************************************/
	// case 3: the child was deleted
	if( deletedChildPos > 0 )
	{// the child node deleted is not at leftmost
		Node->keys.erase( Node->keys.begin() +  deletedChildPos - 1 );
	}
	else
	{// the child node deleted is at leftmost
		Node->keys.erase( Node->keys.begin() +  deletedChildPos );
	}
	Node->childrenPageNums.erase(Node->childrenPageNums.begin() + deletedChildPos);
	Node->children.erase(Node->children.begin() + deletedChildPos);

	// update the position of the children of the current node
	for( i = 0; i < Node->children.size(); i++ )
	{
		if( Node->children[i] != NULL )
		{
			Node->children[i]->pos = i;
		}
	}

	/************************************************************************************************/
	// case 3(i) : the current node has enough entries to be deleted
	if( Node->keys.size() >= this->_order )
	{// usual case, discard the child node
		//cout<<"return to level "<<nodeLevel<<endl;
		//cout<<"delete, usual case"<<endl;
		this->_updated_nodes.push_back(Node);
		deletedChildPos = -1;
		return SUCCESS;
	}

	/*************************************************************************************************/
	//case 3(ii) :the current node has not extra items, need to redistribute or merge with a sibling

	//get a sibling
	int siblingPos = 0;
	int siblingPageNum = 0;
	BTreeNode<KEY>* siblingNode = NULL;
	if( Node->pos < Node->parent->children.size()-1 )
	{// right sibling, if the node is not at rightmost
		//cout<<"get the right sibling ";
		siblingPos = Node->pos + 1;
		siblingPageNum = Node->parent->childrenPageNums[siblingPos];
	}
	else
	{// left sibling, if the node is at rightmost
		//cout<<"get the left sibling ";
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
	//cout<<"and this sibling has "<<siblingNode->children.size()<<" children "<<endl;
	//cout<<"the sibling node has "<<siblingNode->keys.size()<<" items"<<endl;
	if(siblingNode->keys.size()-1 >= this->_order)
	{// if the sibling node has extra entries
		//cout<<"return to the level "<<nodeLevel<<" need to redistribute"<<endl;
		RedistributeNLeafNode(Node,siblingNode);
		deletedChildPos = -1;
		this->_updated_nodes.push_back(Node);
		this->_updated_nodes.push_back(siblingNode);
		this->_updated_nodes.push_back(Node->parent);
		return SUCCESS;
	}
	else
	{// merge the current node with its sibling node
		//cout<<"return to the level "<<nodeLevel<<" need to merge"<<endl;
		BTreeNode<KEY> * leftNode;
		BTreeNode<KEY> * rightNode;

		if( Node->pos < siblingNode->pos)
		{// right sibling
			leftNode = Node;
			rightNode = siblingNode;
			//cout<<"the non leaf node at position "<<deletedChildPos<< " is deleted"<<endl;
		}
		else
		{// left sibling
			leftNode = siblingNode;
			rightNode = Node;
		}

		MergeNode(leftNode,rightNode);
		//cout<<"merge into the current non leaf node"<<endl;
		//cout<<"after merge, the node has "<<Node->children.size()<<" children"<<endl;
		deletedChildPos = rightNode->pos;

		//cout<<"merge into the left sibling node"<<endl;
		//cout<<siblingNode->children[4]->keys.size()<<endl;
		//cout<<"after merge, the  node has "<<siblingNode->children.size()<<" items"<<endl;
		//cout<<"XXXXXXXXXX"<<siblingNode->children[siblingNode->children.size()-2]->keys.size()<<endl;
		//cout<<"the non leaf node at position "<<oldchildPos<< " is deleted"<<endl;
		this->_updated_nodes.push_back(leftNode);
		this->_updated_nodes.push_back(leftNode->parent);
		if(rightNode->pageNum > 0 && !ExistInVector( _deleted_pagenums, (unsigned)(rightNode->pageNum)))
		{
			this->_deleted_pagenums.push_back(rightNode->pageNum);
		    delete rightNode;
		}
		//cout<<"Now there are "<<this->_deleted_pagenums.size()<<" free pages in the B Tree!"<<endl;
		leftNode = NULL;
		rightNode = NULL;
		return SUCCESS;
	}
}

template <typename KEY>
RC BTree<KEY>::DeleteLeafNode(BTreeNode<KEY>* Node, const KEY key,const RID &rid, int& deletedChildPos)
{
	//cout<<"delete in leaf node"<<endl;
	//cout<<"this leaf node has "<<Node->keys.size()<<" items"<<endl;
	/****************************************************************************************/
	// delete the entry in the leaf node
	unsigned i = 0;
	unsigned keysNum = 0;
	keysNum = Node->keys.size();
	for( i = 0; i < keysNum; i++ )
	{
		if( key == Node->keys[i] && Node->rids[i].pageNum ==  rid.pageNum
				&& Node->rids[i].slotNum == rid.slotNum )
		{
			//cout<<"find the key in the leaf node and delete it successfully!"<<endl;
			Node->keys.erase(Node->keys.begin()+i);
			Node->rids.erase(Node->rids.begin()+i);
			break;
		}
	}
	if( i == keysNum )
	{
	   //cout<<"can not find the entry in the leaf node, deletion fails!"<<endl;
	   return ENTRY_NOT_FOUND;
    }
	/*************************************************************************************************/
	// case 1: the root is empty
	if( !this->_height )
	{
		return ENTRY_NOT_FOUND;
	}
	/************************************************************************************************/
	//case 2: the leaf node is the root
	if(this->_height == 1)
	{// the root is a leaf
		keysNum = Node->keys.size();
		if( keysNum > 1 )
		{// the root has more than one entry
			return SUCCESS;
		}
		else
		{// the tree becomes empty
			if( key == Node->keys[0] && Node->rids[0].pageNum ==  rid.pageNum
									&& Node->rids[0].slotNum == rid.slotNum )
			{
				//cout<<"The B tree now is empty!"<<endl;
				this->_height = 0;
				if( Node->pageNum > 0 && !ExistInVector( _deleted_pagenums,(unsigned) Node->pageNum ))
				{
					this->_deleted_pagenums.push_back(Node->pageNum);
				}
				//cout<<"Now there are "<<this->_deleted_pagenums.size()<<" free pages in the B Tree!"<<endl;
				return SUCCESS;
			}
			else
			{
				//cout<<"can not find the entry!"<<endl;
				return ENTRY_NOT_FOUND;
			}
		}
	}
	/**************************************************************************************************/
	// case 3 : delete normally in leaf node
	if( keysNum -1 >= this->_order )
	{// usual case, delete the child node

		deletedChildPos = -1;
		this->_updated_nodes.push_back(Node);
		//cout<<"delete normally"<<endl;
		return SUCCESS;
	}
    /************************************************************************************************/
	// case 4: the node has not enough elements, need to merge or redistribute
	//get a sibling node
	unsigned siblingPos = 0;
    int siblingPageNum = 0;
	BTreeNode<KEY>* siblingNode = NULL;
	if( Node->pos < Node->parent->children.size()-1 )
	{// get the right sibling, if the node is not at rightmost
		siblingPos = Node->pos + 1;
		siblingPageNum = Node->parent->childrenPageNums[siblingPos];
	}
	else
	{// get the left sibling, if the node is at rightmost
		siblingPos = Node->pos - 1;
		siblingPageNum = Node->parent->childrenPageNums[siblingPos];
	}

	if( Node->parent->children[siblingPos] == NULL)
	{// if the sibling is not in memory, fetch it from disk
		siblingNode = this->_func_ReadNode(siblingPageNum,LEAF_NODE);
		siblingNode->parent = Node->parent;
		siblingNode->pos = siblingPos;
		Node->parent->children[siblingPos] = siblingNode;
	}
	else
	{
		siblingNode = Node->parent->children[siblingPos];
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
    	//cout<<"need to redistribute the leaf node with its sibling in position "<<siblingNode->pos<<endl;
	    RedistributeLeafNode(Node,siblingNode);
	    //cout<<"redistribute successfully"<<endl;
	    deletedChildPos = -1;
		this->_updated_nodes.push_back(Node);
		this->_updated_nodes.push_back(siblingNode);
		this->_updated_nodes.push_back(Node->parent);
		return SUCCESS;
	}
	else
	{// if the sibling node hasn't extra entries, merge the current node with its sibling node
		BTreeNode<KEY> * leftNode;
		BTreeNode<KEY> * rightNode;

	    if( Node->pos < siblingNode->pos)
		{// right sibling
			leftNode = Node;
			rightNode = siblingNode;
		    //cout<<"the non leaf node at position "<<oldchildPos<< " is deleted"<<endl;
		}
		else
		{// left sibling
			leftNode = siblingNode;
			rightNode = Node;
		}

		MergeNode(leftNode,rightNode);
		//cout<<"merge into the current non leaf node"<<endl;
		this->_updated_nodes.push_back(leftNode);
		this->_updated_nodes.push_back(leftNode->parent);
		//cout<<"after merge, the node has "<<Node->children.size()<<" children"<<endl;
		deletedChildPos = rightNode->pos;
		if(Node->pageNum > 0 && !ExistInVector( _deleted_pagenums, (unsigned)(rightNode->pageNum)))
	    {
			this->_deleted_pagenums.push_back(rightNode->pageNum);
			delete rightNode;
		}
		//cout<<"Now there are "<<this->_deleted_pagenums.size()<<" free pages in the B Tree!"<<endl;
		leftNode = NULL;
		rightNode = NULL;
		//cout<<"merge into the left sibling node"<<endl;
	    //cout<<siblingNode->children[4]->keys.size()<<endl;
		//cout<<"after merge, the  node has "<<siblingNode->children.size()<<" items"<<endl;
		//cout<<"XXXXXXXXXX"<<siblingNode->children[siblingNode->children.size()-2]->keys.size()<<endl;
		//cout<<"the non leaf node at position "<<oldchildPos<< " is deleted"<<endl;
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
	//cout<<"the height of the tree is: "<<this->_height<<endl;
	if(this->_root->type == LEAF_NODE)
	{
		return DeleteLeafNode(this->_root,key,rid,oldchildPos);
	}
	else
	{
		return DeleteNLeafNode(this->_root,1,key,rid,oldchildPos);
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
vector<unsigned> BTree<KEY>::GetDeletedPageNums() const
{
	return this->_deleted_pagenums;
}

template <typename KEY>
void BTree<KEY>::ClearPendingNodes()
{
	this->_updated_nodes.clear();
	this->_deleted_pagenums.clear();
}

template <typename KEY>
BTreeNode<KEY>* BTree<KEY>::GetRoot() const
{
	return this->_root;
}

template <typename KEY>
unsigned BTree<KEY>::GetHeight() const
{
	return this->_height;
}

template <typename KEY>
KEY* BTree<KEY>::GetMinKey()
{
	KEY* key = NULL;
	BTreeNode<KEY> *node = this->_root;
	for (unsigned index = this->_height; index > 1; index--)
	{
		NodeType nodeType = index > 2 ? NodeType(0) : NodeType(1);
		node = this->_func_ReadNode(node->childrenPageNums[0], nodeType);
	}

	key = &(node->keys[0]);
	if (DEBUG)
		cout << "BTree<KEY>::GetMinKey - Minimum key in current index is " << *key << "." << endl;
	return key;
}

/* ================== Public Functions End ================== */

/********************* Tree Structure End *********************/

/********************* IX_Manager Begin *********************/

IX_Manager::IX_Manager()
{
	_pf_manager = PF_Manager::Instance();
}

IX_Manager::~IX_Manager()
{
	IX_Manager::_ix_manager = 0;
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
	AttrType attrType;
	if (GetAttrType(tableName, attributeName, attrType) != SUCCESS)
	{
		IX_PrintError(CREATE_INDEX_ERROR);
		return CREATE_INDEX_ERROR;
	}

	string fileName = IX_FILE_NAME(tableName, attributeName);
	if (_pf_manager->CreateFile(fileName.c_str()) != SUCCESS)
	{
		IX_PrintError(CREATE_INDEX_ERROR);
		return CREATE_INDEX_ERROR;
	}

	if (this->InitIndexFile(fileName, attrType) != SUCCESS)
	{
		IX_PrintError(CREATE_INDEX_ERROR);
		return CREATE_INDEX_ERROR;
	}

	return SUCCESS;
}

RC IX_Manager::DestroyIndex(const string tableName,      // destroy an index
		  const string attributeName)
{
	if (_pf_manager->DestroyFile(IX_FILE_NAME(tableName, attributeName).c_str()) != SUCCESS)
	{
		IX_PrintError(DESTROY_INDEX_ERROR);
		return DESTROY_INDEX_ERROR;
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
		IX_PrintError(OPEN_INDEX_ERROR);
		return OPEN_INDEX_ERROR;
	}

	return indexHandle.Open(handle);
}

RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)  // close index
{
	PF_FileHandle *handle = indexHandle.GetFileHandle();
	indexHandle.Close();
	if (_pf_manager->CloseFile(*handle) != SUCCESS)
	{
		IX_PrintError(CLOSE_INDEX_ERROR);
		return CLOSE_INDEX_ERROR;
	}
	return SUCCESS;
}

RC IX_Manager::InitIndexFile(const string fileName, const AttrType attrType)
{
	RC rc = SUCCESS;
	PF_FileHandle handle;
	_pf_manager->OpenFile(fileName.c_str(), handle);
	rc = WriteMetadata(handle, 0, 0, 0, attrType);
	_pf_manager->CloseFile(handle);
	return rc;
}

/********************* IX_Manager End *********************/

/********************* IX_IndexHandle Begin *********************/

IX_IndexHandle::IX_IndexHandle()
{
	this->_pf_handle = NULL;
	this->_free_page_num = 0;
	this->_int_index = NULL;
	this->_float_index = NULL;
}
IX_IndexHandle::~IX_IndexHandle()
{
	if (this->_int_index)
		delete this->_int_index;
	if (this->_float_index)
		delete this->_float_index;
}

/* ================== Protected Functions Begin ================== */

// TODO: consider separating this function from IX_IndexHandle class, and pass in a file handle instead -- can also simplify Functor
/**
 * Reads node of nodeType from index file with given page number.
 *
 * @return a loaded node with pos and pointers not being set.
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
		node->children.push_back(NULL);

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
RC IX_IndexHandle::WriteNodes(const vector<BTreeNode<KEY>*> &nodes)
{
	const unsigned KEY_LENGTH = 4;
	const unsigned RID_LENGTH = 8;
	const unsigned PAGE_NUM_LENGTH = 4;
	RC rc = SUCCESS;

	for (unsigned index = 0; index < nodes.size(); index++)
	{
		BTreeNode<KEY> *node = nodes[index];
		void *page = malloc(PF_PAGE_SIZE);

		if (node->pageNum == -1) 	// new node
		{
			if (this->_free_page_num != 0)	// reuse free page
			{
				node->pageNum = this->_free_page_num;
				rc = this->_pf_handle->ReadPage(this->_free_page_num, page);
				if (rc != SUCCESS)
				{
					if (DEBUG)
						cout << "IX_IndexHandle::WriteNodes - Failed to read page " << this->_free_page_num << "." << endl;
					return rc;
				}

				memcpy(&this->_free_page_num, page, 4);
				if (DEBUG)
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
			if (DEBUG)
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

		rc = this->_pf_handle->WritePage(node->pageNum, page);
		free(page);

		if (rc != SUCCESS)
		{
			if (DEBUG)
				cerr << "IX_IndexHandle::WriteNodes - Failed to write page " << node->pageNum << "." << endl;
			return rc;
		}

		if (DEBUG)
			cout << "IX_IndexHandle::WriteNodes - Wrote one " << (node->type == NodeType(1) ? "leaf" : "non-leaf") << " node on page " << node->pageNum << "." << endl;
	}
	return rc;
}

RC IX_IndexHandle::WriteDeletedNodes(const vector<unsigned> &_deleted_pagenums)
{
	RC rc = 0;
	char page[PF_PAGE_SIZE] = {"\0"};
	for( unsigned i = 0; i < _deleted_pagenums.size(); i++ )
	{
		memcpy(page,&this->_free_page_num,4);
		rc = this->_pf_handle->WritePage(_deleted_pagenums[i],page);
		if( rc )
		{
			return rc;
		}
		this->_free_page_num = _deleted_pagenums[i];
	}
	return rc;
}

template <typename KEY>
RC IX_IndexHandle::InitTree(BTree<KEY> **tree)
{
	// initialize tree
	if (this->_height > 0)	// read root from file
	{
		if (DEBUG)
			cout << "IX_IndexHandle::InitTree - Reading the root [at page " << this->_root_page_num << "] for a tree of height " << this->_height << "." << endl;
		NodeType rootNodeType = this->_height == 1 ? NodeType(1) : NodeType(0);
		BTreeNode<KEY> *root = ReadNode<KEY>(this->_root_page_num, rootNodeType);
		*tree = new BTree<KEY>(DEFAULT_ORDER, root, this->_height, this, &IX_IndexHandle::ReadNode<KEY>);
	}
	else	// create an empty tree
	{
		if (DEBUG)
			cout << "IX_IndexHandle::InitTree - Initializing a tree with empty root as a leaf node." << endl;
		*tree = new BTree<KEY>(DEFAULT_ORDER, this, &IX_IndexHandle::ReadNode<KEY>);
	}

	return SUCCESS;
}

RC IX_IndexHandle::LoadMetadata()
{
	RC rc = SUCCESS;

	// read meta-data
	void *page = malloc(PF_PAGE_SIZE);
	rc = this->_pf_handle->ReadPage(0, page);
	if (rc != SUCCESS)
	{
		cerr << "ERROR!" << endl; // TODO: change it!
		return rc;
	}
	unsigned offset = 0;

	memcpy(&this->_root_page_num, (char *)page + offset, 4);
	offset += 4;
	memcpy(&this->_height, (char *)page + offset, 4);
	offset += 4;
	memcpy(&this->_free_page_num, (char *)page + offset, 4);
	offset += 4;
	memcpy(&this->_key_type, (char *)page + offset, 4);
	free(page);

	if (DEBUG)
	{
		cout << "IX_IndexHandle::LoadMetadata - Loaded metadata from index file:" << endl;
		cout << "	Root page #: " << this->_root_page_num << endl;
		cout << "	Height of index tree: " << this->_height << endl;
		cout << "	Free page #: " << this->_free_page_num << endl;
		cout << "	Key type: " << this->_key_type << endl;
	}
	return rc;
}

template <typename KEY>
RC IX_IndexHandle::UpdateMetadata(const BTree<KEY> *tree)
{
	return WriteMetadata(*(this->_pf_handle), tree->GetRoot()->pageNum,
			tree->GetHeight(), this->_free_page_num, this->_key_type);
}

//template <typename KEY>
//BTree<KEY>* IX_IndexHandle::LoadIndex(BTree<KEY> **index)
//{
//	if (*index == NULL)
//		this->InitTree(index);
//	return index;
//}


/* ================== Protected Functions End ================== */

/* ================== Private Functions Begin ================== */

template <typename KEY>
RC IX_IndexHandle::InsertEntry(BTree<KEY> **index, void *key, const RID &rid)
{
	const KEY theKey = *(KEY *)key;

	if (*index == NULL)
		this->InitTree(index);

	RC rc = (*index)->InsertEntry(theKey, rid);
	if (rc != SUCCESS)
		return rc;

	rc = this->WriteNodes((*index)->GetUpdatedNodes());
	(*index)->ClearPendingNodes();

	if (DEBUG)
	{
		cout << "IX_IndexHandle::InsertEntry - Print tree:" << endl;
		PrintTree(*index);
	}
	return rc;
}

template <typename KEY>
RC IX_IndexHandle::DeleteEntry(BTree<KEY> **tree, void* key, const RID &rid)
{
	const KEY theKey = *(KEY *)key;

	if (*tree == NULL)
		this->InitTree(tree);

	RC rc = (*tree)->DeleteEntry(theKey, rid);
	if (rc != SUCCESS)
	{
		return rc;
	}

	rc = this->WriteDeletedNodes((*tree)->GetDeletedPageNums());
	(*tree)->ClearPendingNodes();
	return rc;
}

template <typename KEY>
RC IX_IndexHandle::GetLeftEntry(const BTreeNode<KEY> *node, const unsigned pos, void *key, RID &rid)
{
	if (pos > 0)
	{
		memcpy(key, &(node->keys[pos - 1]), 4);
		rid = RID(node->rids[pos - 1]);
	}
	else if (node->leftPageNum != -1)
	{
		// TODO: update with a leaf node list managing the buffer
		BTreeNode<KEY> *leftNode = node->left;
		if (leftNode == NULL)
			leftNode = this->ReadNode<KEY>(node->leftPageNum, NodeType(1));

		memcpy(key, &(leftNode->keys[leftNode->keys.size() - 1]), 4);
		rid = RID(leftNode->rids[leftNode->rids.size() - 1]);

		if (node->left == NULL)
			delete leftNode;
	}
	else
	{
		return ENTRY_NOT_FOUND;
	}
	return SUCCESS;
}

template <typename KEY>
RC IX_IndexHandle::GetRightEntry(const BTreeNode<KEY> *node, const unsigned pos, void *key, RID &rid)
{
	if (pos + 1 < node->rids.size())
	{
		rid = RID(node->rids[pos + 1]);
		memcpy(key, &(node->keys[pos + 1]), 4);
	}
	else if (node->rightPageNum != -1)
	{
		// TODO: update with a leaf node list managing the buffer
		BTreeNode<KEY> *rightNode = node->right;
		if (rightNode == NULL)
			rightNode = this->ReadNode<KEY>(node->rightPageNum, LEAF_NODE);

		memcpy(key, &(rightNode->keys[0]), 4);
		rid = RID(rightNode->rids[0]);

		if (node->right == NULL)
			delete rightNode;
	}
	else
	{
		return ENTRY_NOT_FOUND;
	}
	if (DEBUG)
		cout << "IX_IndexHandle::GetRightEntry - Found right entry with new key [" << *(KEY *)key << "]." << endl;
	return SUCCESS;
}

template <typename KEY>
RC IX_IndexHandle::GetEntry(BTree<KEY> *index, void *key, const CompOp compOp, RID &rid)
{
	const KEY theKey = *(KEY *)key;

	BTreeNode<KEY> *leafNode;
	unsigned pos = 0;
	RC rc = index->SearchEntry(theKey, &leafNode, pos);
	if (rc != SUCCESS && rc != ENTRY_NOT_FOUND)
	{
		IX_PrintError(SEARCH_ENTRY_ERROR);
		return SEARCH_ENTRY_ERROR;
	}

	// SearchEntry will try to get the position of key value;
	// if not found, position points to the one greater than it if exists
	switch (compOp)
	{
		case EQ_OP:
			if (rc == ENTRY_NOT_FOUND)
				break;
			rid = RID(leafNode->rids[pos]);
			break;
		case LT_OP:
			rc = GetLeftEntry(leafNode, pos, key, rid);
			break;
		case GT_OP:
			if (rc == SUCCESS)
			{
				rc = GetRightEntry(leafNode, pos, key, rid);
			}
			else if (pos < leafNode->keys.size())	// rc == ENTRY_NOT_FOUND
			{
				memcpy(key, &(leafNode->keys[pos]), 4);
				rid = RID(leafNode->rids[pos]);
			}
			break;
		default:
			// LE_OP, GE_OP, NE_OP, NO_OP: this function should not be invoked under these two conditions
			break;
	}

	return rc;
}

/* ================== Private Functions End ================== */

/* ================== Public Functions Begin ================== */

RC IX_IndexHandle::InsertEntry(void *key, const RID &rid)
{
	RC rc = SUCCESS;
	if (this->_key_type == TypeInt)
	{
		rc = this->InsertEntry(&(this->_int_index), key, rid);
	}
	if (this->_key_type == TypeReal)
	{
		rc = this->InsertEntry(&(this->_float_index), key, rid);
	}

	if (rc != SUCCESS)
	{
		IX_PrintError(INSERT_ENTRY_ERROR);
		return INSERT_ENTRY_ERROR;
	}

	return SUCCESS;
}

RC IX_IndexHandle::DeleteEntry(void *key, const RID &rid)
{
	RC rc;
    if ( _key_type == TypeInt )
	{
    	rc = DeleteEntry(&this->_int_index, key, rid);
	}
	else if ( _key_type == TypeReal )
	{
		rc = DeleteEntry(&this->_float_index, key, rid);
	}

    return rc;

}

/**
 * Gets RID considering given key and compOP. This function actually only handle: EQ_OP, LT_OP and GT_OP.
 * Key value will also be updated to latest one associated with the RID.
 *
 * @ return SUCCESS if found and ENTRY_NOT_FOUND if not found, or other error code.
 */
RC IX_IndexHandle::GetEntry(void *key, const CompOp compOp, RID &rid)
{
	RC rc;
    if (this->_key_type == TypeInt)
	{
    	rc = this->GetEntry(this->_int_index, key, compOp, rid);
	}
	else if (this->_key_type == TypeReal)
	{
    	rc = this->GetEntry(this->_float_index, key, compOp, rid);
	}
    return rc;
}

RC IX_IndexHandle::GetMinKey(void *key)
{
	if (DEBUG)
		cout << "IX_IndexHandle::GetMinKey - Getting minimum key in current index." << endl;
    if (this->_key_type == TypeInt)
	{
    	int* iKey = this->_int_index->GetMinKey();
    	memcpy(key, iKey, 4);
	}
	else if (this->_key_type == TypeReal)
	{
		float* fKey = this->_float_index->GetMinKey();
		memcpy(key, fKey, 4);
	}
    return SUCCESS;
}

RC IX_IndexHandle::Open(PF_FileHandle *handle)
{
	if (this->_pf_handle != NULL)
	{
		IX_PrintError(INVALID_OPERATION);
		return INVALID_OPERATION;
	}

	this->_pf_handle = handle;
	RC rc = this->LoadMetadata();

	return rc;
}

RC IX_IndexHandle::Close()
{
	if (this->_pf_handle == NULL)
	{
		IX_PrintError(INVALID_OPERATION);
		return INVALID_OPERATION;
	}

	if (this->_int_index)
	{
		this->UpdateMetadata(this->_int_index);
		delete this->_int_index;
	}
	if (this->_float_index)
	{
		this->UpdateMetadata(this->_float_index);
		delete this->_float_index;
	}
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

bool IX_IndexHandle::IsOpen() const
{
	return (this->_pf_handle != NULL);
}

/* ================== Public Functions End ================== */

/********************* IX_IndexHandle End *********************/

/********************* IX_IndexScan Start *********************/
IX_IndexScan::IX_IndexScan()
{
	this->isOpen = false;
	this->skipValue = NULL;
}

IX_IndexScan::~IX_IndexScan()
{
}

RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
	      CompOp      compOp,
	      void        *value)
{
	if (this->isOpen)
	{
		IX_PrintError(INVALID_OPERATION);
		return INVALID_OPERATION;
	}
	if (!indexHandle.IsOpen())
	{
		IX_PrintError(INVALID_INDEX_HANDLE);
		return INVALID_INDEX_HANDLE;
	}
	if (compOp != NO_OP && value == NULL)
	{
		IX_PrintError(INVALIDE_DATA);
		return INVALIDE_DATA;
	}

	this->indexHandle = new IX_IndexHandle;
	*(this->indexHandle) = indexHandle;
	this->compOp = compOp;

	if (value != NULL)
	{
		if (this->compOp == NE_OP)
		{
			this->skipValue = malloc(4);
			memcpy(this->skipValue, value, 4);
		}
		else
		{
			memcpy(this->keyValue, value, 4);
		}
	}

	this->isOpen = true;
	return SUCCESS;
}

RC IX_IndexScan::CloseScan()
{
	if (!this->isOpen)
	{
		IX_PrintError(INVALID_OPERATION);
		return INVALID_OPERATION;
	}

	this->indexHandle = NULL;
	if (this->skipValue)
		free(this->skipValue);
	this->skipValue = NULL;

	this->isOpen = false;
	return SUCCESS;
}

RC IX_IndexScan::GetNextEntry(RID &rid)
{
	if (this->compOp == NO_OP || this->compOp == NE_OP)
	{
		this->indexHandle->GetMinKey(this->keyValue);
		this->compOp = GE_OP;
	}

	// transform LE_OP to (EQ_OP + LT_OP) and GT_OP to (EQ_OP + GT_OP)
	CompOp op = (this->compOp == LE_OP || this->compOp == GE_OP) ? EQ_OP : this->compOp;
	if (this->indexHandle->GetEntry(this->keyValue, op, rid) != SUCCESS)
	{
		return END_OF_SCAN;
	}
	if (DEBUG)
	{
		cout << "IX_IndexScan::GetNextEntry - Found one entry [" << rid.pageNum << ":" << rid.slotNum << "]." << endl;
	}
	this->compOp = this->compOp == LE_OP ? LT_OP : this->compOp;
	this->compOp = this->compOp == GE_OP ? GT_OP : this->compOp;

	if (this->skipValue != NULL && strcmp(this->keyValue, (char *)this->skipValue) == 0)
		return GetNextEntry(rid);

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
	case ENTRY_NOT_FOUND:
		errMsg = "Entry does not exist.";
		break;
	case INVALID_INDEX_HANDLE:
		errMsg = "Invalid index handle.";
		break;
	}

	cout << "Encountered error: " << errMsg << endl;
}
