
#include "qe.h"

#define DEBUG false

void printValue(void *value, const AttrType &attrType)
{
	unsigned len = 0;
	switch (attrType)
	{
	case TypeInt:
		cout << "(INT) " << *(int *)value << endl;
		break;
	case TypeReal:
		cout << "(REAL) " << *(float *)value << endl;
		break;
	case TypeVarChar:
		memcpy(&len, (char *)value, sizeof(int));
		char *temp = (char *) malloc(len + 1);
		memcpy(temp, (char *)value + sizeof(int), len);
		temp[len] = '\0';
		cout << "(REAL) " << temp << endl;
		free(temp);
		break;
	}
}

void printTuples(void *data, const vector<Attribute> &attrs)
{
	cout << "****Printing tuple begin****" << endl;
	unsigned offset = 0;
	int iValue = 0;
	float fValue = 0.0;
	for (unsigned i = 0; i < attrs.size(); ++i)
	{
		switch (attrs[i].type)
		{
		case TypeInt:
			memcpy(&iValue, (char *)data + offset, sizeof(int));
			cout << attrs[i].name << ": " << iValue << endl;
			offset += sizeof(int);
			break;
		case TypeReal:
			memcpy(&fValue, (char *)data + offset, sizeof(float));
			cout << attrs[i].name << ": " << fValue << endl;
			offset += sizeof(int);
			break;
		case TypeVarChar:
			memcpy(&iValue, (char *)data + offset, sizeof(int));
			offset += sizeof(int);
			char *temp = (char *)malloc(iValue + 1);
			memcpy(temp, (char *)data + offset, iValue);
			offset += iValue;
			temp[iValue] = '\0';
			cout << attrs[i].name << ": " << temp << endl;
			free(temp);
			break;
		}
	}
	cout << "****Printing tuple end****" << endl << endl;
}

unsigned getLength(void* data, vector<Attribute> attrs)
{
	unsigned offset = 0;

	for (unsigned i = 0; i < attrs.size(); ++i)
	{
		unsigned shift = sizeof(int);
		if (attrs[i].type == TypeVarChar)
		{
			unsigned len = 0;
			memcpy(&len, (char *)data + offset, sizeof(int));
			shift += len;
		}
		offset += shift;
	}

	return offset;

}

string itoa(const unsigned in)
{
	const unsigned zero = 48;
	unsigned copy = in + 1;
	string out = "";

	while (copy > 0)
	{
		unsigned digit = copy % 10;
		copy /= 10;

		out = (char)(zero + digit) + out;
	}

	return out;
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
 ) : _left(leftIn), _right(rightIn), _condition(condition), _numPages(numPages), _leftTuple(NULL), _leftValue(NULL)
{
	leftIn->getAttributes(this->_leftAttrs);
	rightIn->getAttributes(this->_rightAttrs);
}

NLJoin::~NLJoin()
{
	if (this->_leftTuple)
		free(this->_leftTuple);
	if (this->_leftValue)
		free(this->_leftValue);
}

RC NLJoin::getNextTuple(void *data)
{
	// get one tuple from left table
	if (this->_leftTuple == NULL)
	{
		this->_leftTuple = malloc(BUFF_SIZE);
		if (this->_left->getNextTuple(this->_leftTuple) == QE_EOF)
		{
			free(this->_leftTuple);
			this->_leftTuple = NULL;
			if (this->_leftValue)
			{
				free(this->_leftValue);
				this->_leftValue = NULL;
			}
			return QE_EOF;
		}
	}

	// get one tuple from right table
	void *rightTuple = malloc(BUFF_SIZE);
	if (this->_right->getNextTuple(rightTuple) == QE_EOF)	// reach the end of right table
	{
		free(rightTuple);

		if (DEBUG)
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

	// prepare values for join condition
	RC rc = getLeftValue();	// get the type of attribute simultaneously
	if (DEBUG)
	{
		cout << "NLJoin::getNextTuple - Left value: ";
		printValue(this->_leftValue, this->_attrType);
	}
	if (rc != SUCCESS)
	{
		free(rightTuple);
		free(this->_leftTuple);
		this->_leftTuple = NULL;
		if (this->_leftValue)
		{
			free(this->_leftValue);
			this->_leftValue = NULL;
		}
		return rc;
	}

	void *rightValue = malloc(BUFF_SIZE);
	getRightValue(rightTuple, rightValue);
	if (DEBUG)
	{
		cout << "NLJoin::getNextTuple - Right value: ";
		printValue(rightValue, this->_attrType);
	}

	// compare
	if (compare(this->_leftValue, this->_condition.op, rightValue, this->_attrType))
	{
		// get lengths of both tuples
		unsigned leftLen = this->getLeftLength();
		unsigned rightLen = this->getRightLength(rightTuple);

		memcpy(data, this->_leftTuple, leftLen);
		memcpy((char *)data + leftLen, rightTuple, rightLen);

		free(rightValue);
		free(rightTuple);
		return SUCCESS;
	}
	free(rightValue);
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

// TODO: need to enhance error handling
RC IndexScan::getNextTuple(void *data)
{
	RID rid;
    int rc = iter->GetNextEntry(rid);
    if(rc == 0)
    {
    	return rm.readTuple(tablename.c_str(), rid, data);
    }
    return QE_EOF;
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
void getAttrValue(const void*data, void*attr_data, const vector<Attribute> attrs, const string selectAttr)
{
	unsigned offset = 0;
	for(unsigned i = 0; i < attrs.size(); i++)
	{
		if(attrs[i].name == selectAttr)
		{
			if(attrs[i].type == TypeInt || attrs[i].type == TypeReal)
			{
				memcpy(attr_data, (char*)data+offset, 4);
			}
			else
			{
				unsigned len = 0;
				memcpy(&len, (char*)data+offset, 4);
				memcpy((char*)attr_data,(char*)data+offset+4,len);
				*((char*)attr_data + len) = '\0'; //add '\0'in the end
			}
			break;
		}
		if(attrs[i].type == TypeInt || attrs[i].type == TypeReal)
	    {
			offset += 4;
	    }
		else
		{
			unsigned len = 0;
			memcpy(&len, (char*)data+offset, 4);
			offset += len + 4;
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

		unsigned offset = 0;
		if (this->condition.rhsValue.type == TypeVarChar)
		{
			offset = sizeof(int);
			unsigned rightLen = *(int *)this->condition.rhsValue.data;
			*((char *)this->condition.rhsValue.data + offset + rightLen) = '\0';
		}

		if( compare(attr_data, this->condition.op,
				(char *)this->condition.rhsValue.data + offset, this->condition.rhsValue.type) )
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
	this->left_data = NULL;
	this->left_attr_data = NULL;

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
}

RC INLJoin::openIndexScan(void *value)
{
	CompOp op = this->condition.op;
	switch(this->condition.op)
	{
	case GE_OP:
		op = LE_OP;
		break;
	case LE_OP:
		op = GE_OP;
		break;
	case LT_OP:
		op = GT_OP;
		break;
	case GT_OP:
		op = LT_OP;
		break;
	default:
		break;
	}
	this->rightIn->setIterator(op, value);
	return SUCCESS;
}

void joinTuples(void* output, void* lInput, void* rInput, vector<Attribute>lAttrs,vector<Attribute>rAttrs )
{
	unsigned lLength = getLength(lInput, lAttrs);
	memcpy(output, lInput,lLength);

	unsigned rLength = getLength(rInput, rAttrs);
	memcpy((char*)output+lLength, (char*)rInput,rLength);
}

RC INLJoin::getNextTuple(void *data)
{
	char right_data[PF_PAGE_SIZE] = "\0";
	char right_attr_data[PF_PAGE_SIZE] = "\0";
	if(this->left_data == NULL)
	{
		this->left_data = malloc(PF_PAGE_SIZE);
		this->left_attr_data = malloc(PF_PAGE_SIZE);
		if (this->leftIn->getNextTuple(this->left_data) != SUCCESS)
		{
			free(this->left_data);
			this->left_data = NULL;
			if (this->left_attr_data)
			{
				free(this->left_attr_data);
				this->left_attr_data = NULL;
			}
			return QE_EOF;
		}
		getAttrValue(this->left_data, this->left_attr_data, this->left_attrs, this->condition.lhsAttr);
		this->openIndexScan(this->left_attr_data);
	}

	while( this->rightIn->getNextTuple(right_data) != QE_EOF)
	{// loop in the inner relation
		getAttrValue(right_data, right_attr_data, this->right_attrs, this->condition.rhsAttr);
		if( compare(this->left_attr_data, this->condition.op, right_attr_data, this->type) )
		{
			//join the two tuples
			joinTuples( data, this->left_data, right_data, this->left_attrs, this->right_attrs );
			return 0;
		}
	}

	free(this->left_data);
	this->left_data = NULL;
	free(this->left_attr_data);
	this->left_attr_data = NULL;
	return getNextTuple(data);
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
	if(this->left_attr_data != NULL)
	{
		free(this->left_attr_data);
		this->left_attr_data = NULL;
	}
}
/*********************************  Index Loop Join class ends ************************************/

/*********************************  Hash Join class begins ***************************************/

unsigned getIntHash(unsigned value, unsigned M)
{
	srand(value);
	return rand() % M;
}

unsigned getRealHash(unsigned value, unsigned M)
{
	const float factor = 0.61803399;
	return (unsigned)((value * factor - floor(value * factor)) * M);
}

/*
 * BKDR Hash
 */
unsigned getStringHash(char* value, unsigned M)
{
	unsigned seed = 131; // 31 131 1313 13131 131313 etc..
	unsigned hash = 0;

	while (*value)
	{
		hash = hash * seed + (*value++);
	}

	return (hash & 0x7FFFFFFF) % M;

}

unsigned getPartitionHash(char* value, AttrType type, unsigned M)
{
	switch(type)
	{
	case TypeInt:
		return getIntHash(*(unsigned*)value, M);
	case TypeReal:
		return getRealHash(*(float*)value, M);
	case TypeVarChar:
		return getStringHash(value, M);
	}
	return 0;
}

unsigned getJoinHash(char* value, AttrType type, unsigned M)
{
	switch(type)
	{
	case TypeInt:
		return getIntHash(*(unsigned*)value, 2 * M) % M;
	case TypeReal:
		return getRealHash(*(float*)value, 2 * M) % M;
	case TypeVarChar:
		return getStringHash(value, 2 * M) % M;
	}
	return 0;
}

HashJoin::HashJoin(Iterator *leftIn,                     // Iterator of input R
		Iterator *rightIn,                               // Iterator of input S
		const Condition &condition,                      // Join condition
		const unsigned numPages) :
		leftIn(leftIn), rightIn(rightIn), condition(condition), htSize(numPages - 1),
		bktSize(numPages - 1), bktNumPtr(0), leftBktPageNum(0), leftBktPageOffset(sizeof(int)), hashOffset(sizeof(int))
{
	this->leftIn->getAttributes(this->left_attrs);
	this->rightIn->getAttributes(this->right_attrs);

	for (unsigned i = 0; i < this->left_attrs.size(); i++)
	{
		if (this->left_attrs[i].name == this->condition.lhsAttr)
		{
			this->attrType = this->left_attrs[i].type;
			break;
		}
	}

	this->partition(this->leftIn, this->left_attrs, this->condition.lhsAttr, LEFT_PARTITION_PREFIX);
	if (DEBUG)
		cout << "HashJoin::HashJoin - Done partition for the left table." << endl;
	this->partition(this->rightIn, this->right_attrs, this->condition.rhsAttr, RIGHT_PARTITION_PREFIX);
	if (DEBUG)
		cout << "HashJoin::HashJoin - Done partition for the right table." << endl;
}

HashJoin::~HashJoin()
{
}

/**
 * Partitions input (iterator) tuples to buckets.
 */
RC HashJoin::partition(Iterator *iterator, const vector<Attribute> &attrs, const string &attr, const string file_prefix)
{
	Bucket defQ;
	defQ.length = sizeof(int);	// space to store last offset of data in bucket
	defQ.data = NULL;
	vector<Bucket> buckets(this->bktSize, defQ);	// need one page of memory to read data

	// create empty files for each bucket of left table	// TODO: create files when necessary
	PF_Manager* pf_manager = PF_Manager::Instance();
	for (unsigned i = 0; i < this->bktSize; i++)
		pf_manager->CreateFile((file_prefix + itoa(i)).c_str());

	unsigned count = 0;
	void* data = malloc(PF_PAGE_SIZE);
	while (iterator->getNextTuple(data) != QE_EOF)
	{
		unsigned length = getLength(data, attrs);
		char* attr_data = (char *) malloc(length);
		getAttrValue(data, attr_data, attrs, attr);
		unsigned hashNum = getPartitionHash(attr_data, this->attrType, this->bktSize);
		free(attr_data);

		// if the bucket is full, write it to disk
		if (buckets[hashNum].length + length > PF_PAGE_SIZE)
			this->writeBucket(buckets, hashNum, file_prefix);

		buckets[hashNum].data = realloc(buckets[hashNum].data, buckets[hashNum].length + length);
	    memcpy((char *)buckets[hashNum].data + buckets[hashNum].length, data, length);
		buckets[hashNum].length += length;
		if (DEBUG)
		{
			count++;
			if (count % 50 == 0)
			{
				cout << "HashJoin::partition - Put data to bucket [" << hashNum << "]." << endl;
				cout << "HashJoin::partition - Hashed " << count << " tuples to this point." << endl;
			}
		}
	}
	free(data);
	if (DEBUG)
		cout << "HashJoin::partition - Hashed " << count << " tuples in total." << endl;

	// flush rest data in the buffer to disk
	for (unsigned i = 0; i < this->bktSize; ++i)
	{
		if (buckets[i].data != NULL)
			this->writeBucket(buckets, i, file_prefix);
	}

	return SUCCESS;
}

// TODO: what if fail to operate files?
/**
 * Append one page to the bucket on disk and resets the corresponding container in hash table.
 * The size of total data on the page is stored at the beginning of the page.
 */
RC HashJoin::writeBucket(vector<Bucket> &ht, const unsigned pos, const string file_prefix)
{
	if (DEBUG)
		cout << "HashJoin::writeBucket - Writing data for bucket [" << pos << "]." << endl;

	PF_Manager* pf_manager = PF_Manager::Instance();
	PF_FileHandle filehandle;
	pf_manager->OpenFile((file_prefix + itoa(pos)).c_str(), filehandle);
	memcpy(ht[pos].data, &ht[pos].length, sizeof(int));
	filehandle.AppendPage(ht[pos].data);
	pf_manager->CloseFile(filehandle);

	// reset the bucket
	free(ht[pos].data);
	ht[pos].data = NULL;
	ht[pos].length = sizeof(int);

	return SUCCESS;
}

RC HashJoin::getNextTuple(void* data)
{
	PF_Manager* pf_manager = PF_Manager::Instance();
	PF_FileHandle right_filehandle;
	PF_FileHandle left_filehandle;

	while (this->bktNumPtr < this->bktSize)
	{
		string right_fileName = RIGHT_PARTITION_PREFIX + itoa(this->bktNumPtr);
		string left_fileName = LEFT_PARTITION_PREFIX + itoa(this->bktNumPtr);

		// find one bucket containing data from the smaller table
		pf_manager->OpenFile(right_fileName.c_str(), right_filehandle);
		if (right_filehandle.GetNumberOfPages() == 0)
		{
			pf_manager->CloseFile(right_filehandle);
			pf_manager->DestroyFile(right_fileName.c_str());
			pf_manager->DestroyFile(left_fileName.c_str());
			this->bktNumPtr++;
			continue;
		}

		// build in-memory hash table for current bucket of the smaller table
		Bucket defQ;
		defQ.length = sizeof(int);	// space to store last offset of data in bucket
		defQ.data = NULL;
		vector<Bucket> hash_table(this->htSize, defQ);
		this->buildHashtable(right_filehandle, hash_table);
		if (DEBUG)
			cout << "HashJoin::getNextTuple - Put bucket [" << this->bktNumPtr << "] of smaller table in memory." << endl;

		// retrieve tuples from the bigger table to perform join
		pf_manager->OpenFile(left_fileName.c_str(), left_filehandle);
		while (this->leftBktPageNum < left_filehandle.GetNumberOfPages())
		{
			void *page = malloc(PF_PAGE_SIZE);
			left_filehandle.ReadPage(this->leftBktPageNum, page);
			unsigned size = 0;
			memcpy(&size, page, sizeof(int));

			// read tuples from current page
			while (this->leftBktPageOffset < size)
			{
				unsigned left_tuple_length = getLength((char *)page + this->leftBktPageOffset, this->left_attrs);
				void *left_tuple = malloc(left_tuple_length);
				memcpy(left_tuple, (char *)page + this->leftBktPageOffset, left_tuple_length);

				// join tuples
				RC rc = this->join(left_tuple, left_tuple_length, hash_table, data);
				free(left_tuple);

				if (rc == SUCCESS)
				{
					free(page);
					for (unsigned i = 0; i < this->htSize; ++i)
						if (hash_table[i].data)
							free(hash_table[i].data);

					// close bucket files
					pf_manager->CloseFile(left_filehandle);
					pf_manager->CloseFile(right_filehandle);

					return SUCCESS;
				}
				this->leftBktPageOffset += left_tuple_length;
			}

			free(page);
			// resets page offset and continue to read next page
			this->leftBktPageOffset = sizeof(int);	// space to store last offset of data in bucket
			this->leftBktPageNum++;
		}

		for (unsigned i = 0; i < this->htSize; ++i)
			if (hash_table[i].data)
				free(hash_table[i].data);

		// no valid join tuple found; resets page number and continue to read next bucket
		this->leftBktPageNum = 0;
		this->bktNumPtr++;

		// close bucket files and remove them
		pf_manager->CloseFile(left_filehandle);
		pf_manager->CloseFile(right_filehandle);
		pf_manager->DestroyFile(left_fileName.c_str());
		pf_manager->DestroyFile(right_fileName.c_str());
	}

	return QE_EOF;
}

RC HashJoin::join(const void *left_tuple, const unsigned left_tuple_length, const vector<Bucket> &hash_table, void *data)
{
	if (DEBUG)
		cout << "HashJoin::join - Trying to join..." << endl;

	// hash left attribute in condition
	char *left_attr_data = (char *) malloc(left_tuple_length);
	getAttrValue(left_tuple, left_attr_data, this->left_attrs, this->condition.lhsAttr);
	unsigned hashNum = getJoinHash(left_attr_data, this->attrType, this->htSize);	// TODO: !!! update it to another hash function !!!

	// if cannot be found in hash table, return
	if (hash_table[hashNum].data == NULL)
	{
		free(left_attr_data);
		return NOT_FOUND;
	}

	unsigned right_tuple_length = 0;
	unsigned offset = this->hashOffset;
	while (offset < hash_table[hashNum].length)
	{
		right_tuple_length = getLength((char *)hash_table[hashNum].data + offset, this->right_attrs);
		void *right_tuple = malloc(right_tuple_length);
		memcpy(right_tuple, (char *)hash_table[hashNum].data + offset, right_tuple_length);

		char *right_attr_data = (char *) malloc(right_tuple_length);
		getAttrValue(right_tuple, right_attr_data, this->right_attrs, this->condition.rhsAttr);
		free(right_tuple);

		if (compare(left_attr_data, EQ_OP, right_attr_data, this->attrType))
		{
			if (DEBUG)
				cout << "HashJoin::join - Found one tuple to join." << endl;
			free(right_attr_data);
			break;
		}
		free(right_attr_data);
		offset += right_tuple_length;
	}
	free(left_attr_data);

	// found one tuple to join
	if (offset < hash_table[hashNum].length)
	{
		memcpy(data, left_tuple, left_tuple_length);
		memcpy((char *)data + left_tuple_length, (char *)hash_table[hashNum].data + offset, right_tuple_length);
		this->hashOffset = offset + right_tuple_length;
		return SUCCESS;
	}

	this->hashOffset = sizeof(int);
	return NOT_FOUND;
}

RC HashJoin::buildHashtable(PF_FileHandle &filehandle, vector<Bucket> &hash_table)
{
	for (unsigned pageNum = 0; pageNum < filehandle.GetNumberOfPages(); ++pageNum)
	{
		void *page = malloc(PF_PAGE_SIZE);
		filehandle.ReadPage(pageNum, page);
		unsigned size = 0;
		memcpy(&size, page, sizeof(int));

		unsigned offset = sizeof(int);	// space to store last offset of data in bucket
		while (offset < size)
		{
			unsigned length = getLength((char *)page + offset, this->right_attrs);
			void *tuple = malloc(length);
			memcpy(tuple, (char *)page + offset, length);
			offset += length;

			char *attr_data = (char *) malloc(length);
			getAttrValue(tuple, attr_data, this->right_attrs, this->condition.rhsAttr);
			unsigned hashNum = getJoinHash(attr_data, this->attrType, this->htSize);	// TODO: !!! update it to another hash function !!!
			hash_table[hashNum].data = realloc(hash_table[hashNum].data, hash_table[hashNum].length + length);
			memcpy((char *)hash_table[hashNum].data + hash_table[hashNum].length, tuple, length);
			hash_table[hashNum].length += length;

			free(attr_data);
			free(tuple);
		}
		free(page);
	}

	return SUCCESS;
}

void HashJoin::getAttributes(vector<Attribute> &attrs) const
{
	attrs.clear();
	attrs = this->left_attrs;
	attrs.insert(attrs.end(), this->right_attrs.begin(), this->right_attrs.end());
}

/*********************************  Hash Join class ends ******************************************/

