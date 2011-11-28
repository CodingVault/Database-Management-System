
# include "qe.h"
# include <iostream>

#define DEBUG true

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
		if (rc != SUCCESS)
			return rc;

		char *rightValue;
		if (this->_condition.bRhsIsAttr)
		{
			rightValue = (char *) malloc(BUFF_SIZE);
			getRightValue(rightTuple, rightValue);
		}

		// append end of string
		unsigned leftLen = this->getLeftLength();
		unsigned rightLen = this->getRightLength(rightTuple);
		*((char *)this->_leftValue + leftLen) = '\0';
		if (rightValue)
			rightValue[rightLen] = '\0';
		else
			*((char *)this->_condition.rhsValue.data + rightLen) = '\0';

		// compare
		if ((rightValue && compare(this->_leftValue, this->_condition.op, rightValue, this->_attrType)) ||
				(!rightValue && compare(this->_leftValue, this->_condition.op, this->_condition.rhsValue.data, this->_attrType)))
		{
			memcpy(data, this->_leftTuple, leftLen);
			memcpy((char *)data + leftLen, rightTuple, rightLen);
		}
		if (rightValue)
			free(rightValue);
	}
	else
	{
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

	return SUCCESS;
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
			unsigned len = 0;
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

			this->_leftValue = malloc(len);
			memcpy(this->_leftValue, (char *)this->_leftTuple + offset, len);
			return SUCCESS;
		}
		offset += len;
	}

	return SUCCESS;
}

void NLJoin::getRightValue(void *rightTuple, void *value)
{
	unsigned offset = 0;
	for (unsigned i = 0; i < this->_rightAttrs.size(); ++i)
	{
		unsigned len = sizeof(int);
		if (this->_rightAttrs[i].type == TypeVarChar)
		{
			unsigned len = 0;
			memcpy(&len, (char *)this->_leftTuple + offset, sizeof(int));
			offset += sizeof(int);
		}

		if (this->_condition.rhsAttr == this->_rightAttrs[i].name)
		{
			memcpy(value, (char *)rightTuple + offset, len);
			return;
		}
		offset += len;
	}
}
