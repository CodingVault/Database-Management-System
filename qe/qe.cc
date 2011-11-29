
# include "qe.h"
# include <iostream>

#define DEBUG true

void printValue(void *value, const AttrType &attrType)
{
	switch (attrType)
	{
	case TypeInt:
		cout << "(INT) ";
		cout << *(int *)value << endl;
		break;
	case TypeReal:
		cout << "(REAL) ";
		cout << *(float *)value << endl;
		break;
	case TypeVarChar:
		cout << "(VAR_CHAR) ";
		cout << (char *)value << endl;
		break;
	}
}

/************************* Project Begin *************************/

Project::Project(Iterator *input, const vector<string> &attrNames)
{
	this->_iter = input;
	this->_attrNames = attrNames;
	input->getAttributes(this->_attrs);
}

Project::~Project()
{
}

RC Project::getNextTuple(void *data)
{
	if (this->_iter->getNextTuple(data) != QE_EOF)
	{
		if (DEBUG)
			cout << "Project::getNextTuple - Got one tuple to process..." << endl;

		unsigned offset = 0;
		vector<Buff> buffer(this->_attrNames.size());

		for (unsigned i = 0; i < this->_attrs.size(); ++i)
		{
			unsigned shift = sizeof(int);
			if (this->_attrs[i].type == TypeVarChar)
			{
				unsigned len = 0;
				memcpy(&len, (char *)data + offset, sizeof(int));
				shift += len;
			}

			// store projected values in buffer with the same sequence as this->_attrNames
			vector<string>::iterator it = find(this->_attrNames.begin(), this->_attrNames.end(), this->_attrs[i].name);
			if (it != this->_attrNames.end())
			{
				if (DEBUG)
					cout << "Project::getNextTuple - Found value for " << this->_attrs[i].name << " [length: " << shift << "]." << endl;

				Buff buff;
				buff.length = shift;
				buff.data = malloc(shift);
				memcpy(buff.data, (char *)data + offset, shift);
				size_t pos = it - this->_attrNames.begin();
				buffer[pos] = buff;
			}

			offset += shift;
		}

		// flush buffer to output
		offset = 0;
		for (unsigned i = 0; i < buffer.size(); ++i)
		{
			memcpy((char *)data + offset, buffer[i].data, buffer[i].length);
			offset += buffer[i].length;

			free(buffer[i].data);
		}

		return SUCCESS;
	}

	return QE_EOF;
}

void Project::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = vector<Attribute>(this->_attrNames.size());

    for(unsigned i = 0; i < this->_attrs.size(); ++i)
    {
    	for (unsigned j = 0; j < this->_attrNames.size(); ++j)
    	{
    		if (this->_attrs[i].name == this->_attrNames[j])
    		{
    			attrs[j] = this->_attrs[i];
    			break;
    		}
    	}
    }
}

/************************* Project End *************************/

/************************* NLJoin Begin *************************/

NLJoin::NLJoin(Iterator *leftIn,                      // Iterator of input R
        TableScan *rightIn,                           // TableScan Iterator of input S
        const Condition &condition,                   // Join condition
        const unsigned numPages                       // Number of pages can be used to do join (decided by the optimizer)
 )
{
	this->_left = leftIn;
	this->_right = rightIn;
	this->_condition = condition;
	this->_numPages = numPages;
	this->_leftTuple = NULL;
	this->_leftValue = NULL;

	leftIn->getAttributes(this->_leftAttrs);
	rightIn->getAttributes(this->_rightAttrs);
}

NLJoin::~NLJoin()
{
	if (this->_leftTuple)
		free(this->_leftTuple);
}

RC NLJoin::getNextTuple(void *data)
{
	// get one tuple from left table
	if (this->_leftTuple == NULL)
	{
		this->_leftTuple = malloc(BUFF_SIZE);
		if (this->_left->getNextTuple(this->_leftTuple) == QE_EOF)
			return QE_EOF;
	}

	// get one tuple from right table
	void *rightTuple = malloc(BUFF_SIZE);
	if (this->_right->getNextTuple(rightTuple) != QE_EOF)
	{
		// prepare values for join condition
		RC rc = getLeftValue();	// get the type of attribute simultaneously
		if (DEBUG)
		{
			cout << "NLJoin::getNextTuple - Left value: ";
			printValue(this->_leftValue, this->_attrType);
		}
		if (rc != SUCCESS)
			return rc;

		char *rightValue;
		rightValue = (char *) malloc(BUFF_SIZE);
		getRightValue(rightTuple, rightValue);
		if (DEBUG)
		{
			cout << "NLJoin::getNextTuple - Right value: ";
			printValue(rightValue, this->_attrType);
		}

		// get lengths of both tuples
		unsigned leftLen = this->getLeftLength();
		unsigned rightLen = this->getRightLength(rightTuple);

		// compare
		if (compare(this->_leftValue, this->_condition.op, rightValue, this->_attrType))
		{
			memcpy(data, this->_leftTuple, leftLen);
			memcpy((char *)data + leftLen, rightTuple, rightLen);

			return SUCCESS;
		}
		free(rightValue);
	}
	else
	{
		cout << "NLJoin::getNextTuple - Preparing for reading next left tuple." << endl;
		free(this->_leftTuple);
		this->_leftTuple = NULL;
		if (this->_leftValue)
		{
			free(this->_leftValue);
			this->_leftValue = NULL;
		}
		this->_right->setIterator();
		return getNextTuple(data);
	}
	free(rightTuple);

	return getNextTuple(data);
}

void NLJoin::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->_leftAttrs;
	attrs.insert(attrs.end(), this->_rightAttrs.begin(), this->_rightAttrs.end());
}

unsigned NLJoin::getLeftLength()
{
	unsigned offset = 0;

	for (unsigned i = 0; i < this->_leftAttrs.size(); ++i)
	{
		unsigned shift = sizeof(int);
		if (this->_leftAttrs[i].type == TypeVarChar)
		{
			unsigned len = 0;
			memcpy(&len, (char *)this->_leftTuple + offset, sizeof(int));
			shift += len;
		}
		offset += shift;
	}

	return offset;
}

unsigned NLJoin::getRightLength(void *rightTuple)
{
	unsigned offset = 0;

	for (unsigned i = 0; i < this->_rightAttrs.size(); ++i)
	{
		unsigned shift = sizeof(int);
		if (this->_rightAttrs[i].type == TypeVarChar)
		{
			unsigned len = 0;
			memcpy(&len, (char *)rightTuple + offset, sizeof(int));
			shift += len;
		}
		offset += shift;
	}

	return offset;
}

RC NLJoin::getLeftValue()
{
	if (this->_leftValue != NULL)
		return SUCCESS;

	unsigned offset = 0;
	for (unsigned i = 0; i < this->_leftAttrs.size(); ++i)
	{
		unsigned len = sizeof(int);
		if (this->_leftAttrs[i].type == TypeVarChar)
		{
			memcpy(&len, (char *)this->_leftTuple + offset, sizeof(int));
			offset += sizeof(int);
		}

		if (this->_condition.lhsAttr == this->_leftAttrs[i].name)
		{
			this->_attrType = this->_leftAttrs[i].type;
			if (!this->_condition.bRhsIsAttr && this->_condition.rhsValue.type != this->_attrType)
			{
				cerr << "Error: unmatched attribute type - input condition is [" << this->_condition.rhsValue.type << "], ";
				cerr << "but found [" << this->_attrType << "] according to Condition.lhsAttr." << endl;
				return INCOMPLIANCE;
			}
			if (DEBUG)
				cout << "NLJoin::getLeftValue - Found " << this->_condition.lhsAttr << " [type: " << this->_attrType << "; length: " << len << "]" << endl;

			this->_leftValue = malloc(len);
			memcpy(this->_leftValue, (char *)this->_leftTuple + offset, len);
			*((char *)this->_leftValue + len) = '\0';
			return SUCCESS;
		}
		offset += len;
	}

	return SUCCESS;
}

void NLJoin::getRightValue(void *rightTuple, void *value)
{
	if (this->_condition.bRhsIsAttr)	// get right value from right tuple according to given attribute name
	{
		unsigned offset = 0;
		for (unsigned i = 0; i < this->_rightAttrs.size(); ++i)
		{
			unsigned len = sizeof(int);
			if (this->_rightAttrs[i].type == TypeVarChar)
			{
				memcpy(&len, (char *)rightTuple + offset, sizeof(int));
				offset += sizeof(int);
			}

			if (this->_condition.rhsAttr == this->_rightAttrs[i].name)
			{
				if (DEBUG)
					cout << "NLJoin::getRightValue - Found " << this->_condition.rhsAttr << " [type: " << this->_rightAttrs[i].type << "; length: " << len << "]" << endl;
				memcpy(value, (char *)rightTuple + offset, len);
				*((char *)value + len) = '\0';
				return;
			}
			offset += len;
		}
	}
	else		// get right value from Value in Condition
	{
		unsigned offset = 0;
		unsigned len = sizeof(int);
		if (this->_condition.rhsValue.type == TypeVarChar)
		{
			memcpy(&len, this->_condition.rhsValue.data, sizeof(int));
			offset += sizeof(int);
		}
		memcpy(value, (char *)this->_condition.rhsValue.data + offset, len);
		*((char *)value + len) = '\0';
	}
}

/************************* NLJoin End *************************/

/*********************************  IndexScan class begins **********************************************************/
IndexScan::IndexScan(RM &rm, const IX_IndexHandle &indexHandle, const string tablename, const char *alias):rm(rm)
{
	// Get Attributes from RM
    rm.getAttributes(tablename, attrs);

    // Store tablename
    this->tablename = tablename;
    if(alias) this->tablename = string(alias);

    // Store Index Handle
    iter = NULL;
    this->handle = indexHandle;
}

void IndexScan::setIterator(CompOp compOp, void *value)
{
	if(iter != NULL)
    {
		iter->CloseScan();
        delete iter;
    }
    iter = new IX_IndexScan();
    iter->OpenScan(handle, compOp, value);
}

RC IndexScan::getNextTuple(void *data)
{
	RID rid;
    int rc = iter->GetNextEntry(rid);
    if(rc == 0)
    {
    	rc = rm.readTuple(tablename.c_str(), rid, data);
    }
    return rc;
}

void IndexScan::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
    attrs = this->attrs;
    unsigned i;

    // For attribute in vector<Attribute>, name it as rel.attr
    for(i = 0; i < attrs.size(); ++i)
    {
    	string tmp = tablename;
        tmp += ".";
        tmp += attrs[i].name;
        attrs[i].name = tmp;
    }
}

IndexScan::~IndexScan()
{
	iter->CloseScan();
}
/*********************************  IndexScan class ends   **********************************************************/

/*********************************  Filter class begins **********************************************************/
/*
 * get a specified attribute in a tuple
 */
void getAttrValue(void*data, void*attr_data, vector<Attribute> attrs, string selectAttr)
{
	unsigned offset = 0;
	for(unsigned i = 0; i < attrs.size(); i++)
	{
		if(attrs[i].name == selectAttr)
		{
			if(attrs[i].type == TypeInt || attrs[i].type == TypeReal)
			{
				memcpy((char*)attr_data,(char*)data+offset,4);
			}
			else
			{
				memcpy((char*)attr_data,(char*)data+offset+4,*((int*)(data)+offset));
				memcpy((char*)attr_data + *((int*)(data)+offset), &("\0"),1); //add '\0'in the end
			}
			break;
		}
		if(attrs[i].type == TypeInt || attrs[i].type == TypeReal)
	    {
			offset += 4;
	    }
		else
		{
			offset += *((int*)(data) + offset) + 4;
		}
	}
}

Filter::Filter(Iterator *input, const Condition &condition)
{
	this->input = input;
	this->input->getAttributes(this->attrs);
	this->condition = condition;
}

RC Filter::getNextTuple(void *data)
{
	char attr_data[PF_PAGE_SIZE] = "\0";
	while(this->input->getNextTuple(data) != QE_EOF)
	{
		// analyze the tuple
		getAttrValue(data, attr_data, this->attrs, this->condition.lhsAttr);
		if( compare(attr_data, this->condition.op,
				this->condition.rhsValue.data, this->condition.rhsValue.type) )
		{
			return 0;
		}
	};
	//reach the end of the input iterator
	return QE_EOF;
}

void Filter::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->attrs;
}

Filter::~Filter()
{
	this->input = NULL;
}
/*********************************  Filter class ends **********************************************************/


/*********************************  Index Loop Join class begins **************************************************/
INLJoin::INLJoin (Iterator *leftIn,                               // Iterator of input R
             IndexScan *rightIn,                             // IndexScan Iterator of input S
             const Condition &condition,                     // Join condition
             const unsigned numPages                         // Number of pages can be used to do join (decided by the optimizer)
     )
{
	this->leftIn = leftIn;
	this->rightIn = rightIn;
	this->condition = condition;
	this->numPages = numPages;
	this->left_data = ( char* )malloc(PF_PAGE_SIZE);

	this->leftIn->getAttributes(this->left_attrs);
	this->rightIn->getAttributes(this->right_attrs);

	for(unsigned i = 0; i < this->left_attrs.size(); i++)
	{
		if(this->left_attrs[i].name == this->condition.lhsAttr)
		{
			this->type = this->left_attrs[i].type; //TODO: how about the type of left and right attribute are different
			break;
		}
	}

	if(this->leftIn->getNextTuple(this->left_data) == QE_EOF)
	{// get the first tuple in the outer relation
		this->left_data = NULL;
	}
}

void joinTuples(void* output, void* lInput, void* rInput, vector<Attribute>lAttrs,vector<Attribute>rAttrs )
{
	unsigned lLength = 0, rLength = 0, i = 0;
	for(i = 0; i < lAttrs.size(); i++)
	{// write the left tuple
		if(lAttrs[i].type == TypeInt || lAttrs[i].type == TypeReal)
		{
			lLength += 4;
		}
		else
		{
			lLength += *((int*)(lInput) + lLength) + 4;
		}
	}

	memcpy(output, lInput,lLength);

	for(i = 0; i < rAttrs.size(); i++)
	{// write the left tuple
		if(rAttrs[i].type == TypeInt || rAttrs[i].type == TypeReal)
		{
			rLength += 4;
		}
		else
		{
			rLength += *((int*)(rInput) + rLength) + 4;
		}
	}
	memcpy((char*)output+lLength, (char*)rInput,rLength);
}

RC INLJoin::getNextTuple(void *data)
{
	char right_data[PF_PAGE_SIZE] = "\0";
	char right_attr_data[PF_PAGE_SIZE] = "\0";
	char left_attr_data[PF_PAGE_SIZE] = "\0";
	if(this->left_data == NULL)
	{
		return QE_EOF;
	}

	do{
		getAttrValue(this->left_data, left_attr_data, this->left_attrs, this->condition.lhsAttr);
		while( this->rightIn->getNextTuple(right_data) != QE_EOF)
	    {// loop in the inner relation
		    getAttrValue(right_data, right_attr_data, this->right_attrs, this->condition.rhsAttr);
		    if( compare(left_attr_data, this->condition.op,
			    	right_attr_data, this->type) )
		    {
		    	//join the two tuples
		    	joinTuples( data, this->left_data, right_data, this->left_attrs, this->right_attrs );
			    return 0;
		    }
	    }
		// reset the inner iterator
		this->rightIn->setIterator(NO_OP,NULL);
	}while(this->leftIn->getNextTuple(this->left_data) != QE_EOF);

	return QE_EOF;
}

void INLJoin::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->left_attrs;
	attrs.insert(attrs.end(), this->right_attrs.begin(), this->right_attrs.end());
}

INLJoin::~INLJoin()
{
	this->leftIn = NULL;
	this->rightIn = NULL;
	this->left_attrs.clear();
	this->right_attrs.clear();
	if(this->left_data != NULL)
	{
		free(this->left_data);
		this->left_data = NULL;
	}
}
/*********************************  Index Loop Join class ends **************************************************/



/*********************************  Hash Join class begins **************************************************/


/*********************************  Hash Join class ends **************************************************/

