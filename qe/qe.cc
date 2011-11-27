
# include "qe.h"


Project::Project(Iterator *input, const vector<string> &attrNames)
{
	this->_iter = input;
	this->_attrNames = attrNames;
}

RC Project::getNextTuple(void *data)
{
	vector<Attribute> attrs;
	this->_iter->getAttributes(attrs);

	if (this->_iter->getNextTuple(data) != QE_EOF)
	{
		unsigned inPos = 0;
		unsigned outPos = 0;

		for (unsigned i = 0; i < attrs.size(); ++i)
		{
			unsigned shift = 4;
			if (attrs[i].type == TypeVarChar)
			{
				unsigned len = *((int *)data + inPos);
				shift += len;
			}

			if (find(this->_attrNames.begin(), this->_attrNames.end(), attrs[i].name)
					!= this->_attrNames.end())
			{
				memcpy((char *)data + outPos, (char *)data + inPos, shift);
				outPos += shift;
			}
			inPos += shift;
		}
		return SUCCESS;
	}

	return QE_EOF;
}

void Project::getAttributes(vector<Attribute> &attrs) const
{
    this->_iter->getAttributes(attrs);

    for(unsigned i = 0; i < attrs.size(); ++i)
    {
    	if (find(this->_attrNames.begin(), this->_attrNames.end(), attrs[i].name)
    			== this->_attrNames.end())
    		attrs.erase(attrs.begin() + i);
    }
}
