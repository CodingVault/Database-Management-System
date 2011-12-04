#ifndef _qe_h_
#define _qe_h_

#include <vector>
#include <iostream>
#include <cmath>

#include "../pf/pf.h"
#include "../rm/rm.h"
#include "../ix/ix.h"

#define QE_EOF (-1)  // end of the index scan
#define INCOMPLIANCE (-2)
#define NOT_FOUND (-3)
#define BUFF_SIZE PF_PAGE_SIZE

using namespace std;

#define LEFT_PARTITION_PREFIX "HJ_LEFT_PAR_"
#define RIGHT_PARTITION_PREFIX "HJ_RIGHT_PAR_"

typedef enum{ MIN = 0, MAX, SUM, AVG, COUNT } AggregateOp;


// The following functions use  the following 
// format for the passed data.
//    For int and real: use 4 bytes
//    For varchar: use 4 bytes for the length followed by
//                          the characters

struct Value {
    AttrType type;          // type of value               
    void     *data;         // value                       
};

struct Buff {
	AttrLength length;
	void       *data;
};

struct Bucket {
	unsigned length;
	void     *data;
};

struct Condition {
    string lhsAttr;         // left-hand side attribute                     
    CompOp  op;             // comparison operator                          
    bool    bRhsIsAttr;     // TRUE if right-hand side is an attribute and not a value; FALSE, otherwise.
    string rhsAttr;         // right-hand side attribute if bRhsIsAttr = TRUE
    Value   rhsValue;       // right-hand side value if bRhsIsAttr = FALSE
};


class Iterator {
    // All the relational operators and access methods are iterators.
    public:
        virtual RC getNextTuple(void *data) = 0;
        virtual void getAttributes(vector<Attribute> &attrs) const = 0;
        virtual ~Iterator() {};
};


class TableScan : public Iterator
{
    // A wrapper inheriting Iterator over RM_ScanIterator
    public:
        RM &rm;
        RM_ScanIterator *iter;
        string tablename;
        vector<Attribute> attrs;
        vector<string> attrNames;
        
        TableScan(RM &rm, const string tablename, const char *alias = NULL):rm(rm)
        {
            // Get Attributes from RM
            rm.getAttributes(tablename, attrs);

            // Get Attribute Names from RM
            unsigned i;
            for(i = 0; i < attrs.size(); ++i)
            {
                // convert to char *
                attrNames.push_back(attrs[i].name);
            }
            // Call rm scan to get iterator
            iter = new RM_ScanIterator();
            rm.scan(tablename, "", NO_OP, NULL, attrNames, *iter);

            // Store tablename
            this->tablename = tablename;
            if(alias) this->tablename = alias;
        };
       
        // Start a new iterator
        void setIterator()
        {
            iter->close();
            delete iter;
            iter = new RM_ScanIterator();
            rm.scan(tablename, "", NO_OP, NULL, attrNames, *iter);
        };
       
        RC getNextTuple(void *data)
        {
            RID rid;
            return iter->getNextTuple(rid, data);
        };
        
        void getAttributes(vector<Attribute> &attrs) const
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
        };
        
        ~TableScan() 
        {
            iter->close();
            delete iter;
        };
};


class IndexScan : public Iterator
{
    // A wrapper inheriting Iterator over IX_IndexScan
    public:
        IndexScan(RM &rm, const IX_IndexHandle &indexHandle,
        		const string tablename, const char *alias = NULL);

        // Start a new iterator given the new compOp and value
        void setIterator(CompOp compOp, void *value);

        RC getNextTuple(void *data);

        void getAttributes(vector<Attribute> &attrs) const;

        ~IndexScan();

    private:
        RM &rm;
        IX_IndexScan *iter;
        IX_IndexHandle handle;
        string tablename;
        vector<Attribute> attrs;
};


class Filter : public Iterator {
    // Filter operator
    public:
        Filter(Iterator *input,                         // Iterator of input R
               const Condition &condition               // Selection condition 
        );
        ~Filter();
        
        RC getNextTuple(void *data);
        // For attribute in vector<Attribute>, name it as rel.attr
        void getAttributes(vector<Attribute> &attrs) const;
    private:
        Iterator* input;
        vector<Attribute> attrs;
        Condition condition;
};



class Project : public Iterator {
    // Projection operator
    public:
        Project(Iterator *input,                            // Iterator of input R
                const vector<string> &attrNames);           // vector containing attribute names
        ~Project();
        
        RC getNextTuple(void *data);
        // For attribute in vector<Attribute>, name it as rel.attr
        void getAttributes(vector<Attribute> &attrs) const;

    private:
        Iterator *_iter;
        vector<string> _attrNames;
    	vector<Attribute> _attrs;
};


class NLJoin : public Iterator {
    // Nested-Loop join operator
    public:
        NLJoin(Iterator *leftIn,                             // Iterator of input R
               TableScan *rightIn,                           // TableScan Iterator of input S
               const Condition &condition,                   // Join condition
               const unsigned numPages                       // Number of pages can be used to do join (decided by the optimizer)
        );
        ~NLJoin();
        
        RC getNextTuple(void *data);
        // For attribute in vector<Attribute>, name it as rel.attr
        void getAttributes(vector<Attribute> &attrs) const;

    protected:
        unsigned getLeftLength();
        unsigned getRightLength(void *rightTuple);
        RC getLeftValue();
        void getRightValue(void *rightTuple, void *value);

    private:
        Iterator *_left;
        TableScan *_right;
        Condition _condition;
        unsigned _numPages;
    	vector<Attribute> _leftAttrs;
    	vector<Attribute> _rightAttrs;
        AttrType _attrType;
        void *_leftTuple;
        void *_leftValue;
};


class INLJoin : public Iterator {
    // Index Nested-Loop join operator
    public:
        INLJoin(Iterator *leftIn,                               // Iterator of input R
                IndexScan *rightIn,                             // IndexScan Iterator of input S
                const Condition &condition,                     // Join condition
                const unsigned numPages                         // Number of pages can be used to do join (decided by the optimizer)
        );
        
        ~INLJoin();

        RC getNextTuple(void *data);
        // For attribute in vector<Attribute>, name it as rel.attr
        void getAttributes(vector<Attribute> &attrs) const;

    protected:
        RC openIndexScan(void* value);

    private:
        Iterator* leftIn;
        IndexScan* rightIn;

        vector<Attribute> left_attrs;
        vector<Attribute> right_attrs;

        void* left_data;
        void* left_attr_data;

        Condition condition;
        unsigned numPages;
        AttrType type;
};


class HashJoin : public Iterator {
    // Hash join operator
    public:
        HashJoin(Iterator *leftIn,                                // Iterator of input R
                 Iterator *rightIn,                               // Iterator of input S
                 const Condition &condition,                      // Join condition
                 const unsigned numPages                          // Number of pages can be used to do join (decided by the optimizer)
        );
        
        ~HashJoin();

        RC getNextTuple(void *data);
        // For attribute in vector<Attribute>, name it as rel.attr
        void getAttributes(vector<Attribute> &attrs) const;

    protected:
        RC writeBucket(vector<Bucket> &ht, const unsigned pos, const string file_prefix);
        RC partition(Iterator *iterator, const vector<Attribute> &attrs, const string &attr, const string file_prefix);
        RC buildHashtable(PF_FileHandle &filehandle, vector<Bucket> &hash_table);
        RC join(const void *left_tuple, const unsigned left_tuple_length, const vector<Bucket> &hash_table, void *data);

    private:
        Iterator* leftIn;
        Iterator* rightIn;
        Condition condition;
        unsigned bufferSize;
        AttrType attrType;
        vector<Attribute> left_attrs;
        vector<Attribute> right_attrs;

        unsigned bktNum;
        unsigned leftBktPageNum;
        unsigned leftBktPageOffset;
};


class Aggregate : public Iterator {
    // Aggregation operator
    public:
        Aggregate(Iterator *input,                              // Iterator of input R
                  Attribute aggAttr,                            // The attribute over which we are computing an aggregate
                  AggregateOp op                                // Aggregate operation
        );

        // Extra Credit
        Aggregate(Iterator *input,                              // Iterator of input R
                  Attribute aggAttr,                            // The attribute over which we are computing an aggregate
                  Attribute gAttr,                              // The attribute over which we are grouping the tuples
                  AggregateOp op                                // Aggregate operation
        );     

        ~Aggregate();
        
        RC getNextTuple(void *data) {return QE_EOF;};
        // Please name the output attribute as aggregateOp(aggAttr)
        // E.g. Relation=rel, attribute=attr, aggregateOp=MAX
        // output attrname = "MAX(rel.attr)"
        void getAttributes(vector<Attribute> &attrs) const;
};

#endif
