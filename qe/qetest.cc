#include <fstream>
#include <iostream>
#include <cassert>

#include <vector>

#include "qe.h"

using namespace std;

// Global Initialization
RM *rm = RM::Instance();
IX_Manager *ixManager = IX_Manager::Instance();

const int success = 0;

// Number of tuples in each relation
const int tuplecount = 1000;

// Buffer size and character buffer size
const unsigned bufsize = 200;

void createLeftTable()
{
    // Functions Tested;
    // 1. Create Table
    cout << "****Create Left Table****" << endl;

    vector<Attribute> attrs;

    Attribute attr;
    attr.name = "A";
    attr.type = TypeInt;
    attr.length = 4;
    attrs.push_back(attr);

    attr.name = "B";
    attr.type = TypeInt;
    attr.length = 4;
    attrs.push_back(attr);

    attr.name = "C";
    attr.type = TypeReal;
    attr.length = 4;
    attrs.push_back(attr);

    RC rc = rm->createTable("left", attrs);
    assert(rc == success);
    cout << "****Left Table Created!****" << endl;
}

   
void createRightTable()
{
    // Functions Tested;
    // 1. Create Table
    cout << "****Create Right Table****" << endl;

    vector<Attribute> attrs;

    Attribute attr;
    attr.name = "B";
    attr.type = TypeInt;
    attr.length = 4;
    attrs.push_back(attr);

    attr.name = "C";
    attr.type = TypeReal;
    attr.length = 4;
    attrs.push_back(attr);

    attr.name = "D";
    attr.type = TypeInt;
    attr.length = 4;
    attrs.push_back(attr);

    RC rc = rm->createTable("right", attrs);
    assert(rc == success);
    cout << "****Right Table Created!****" << endl;
}


// Prepare the tuple to left table in the format conforming to Insert/Update/ReadTuple and readAttribute
void prepareLeftTuple(const int a, const int b, const float c, void *buf)
{    
    int offset = 0;
    
    memcpy((char *)buf + offset, &a, sizeof(int));
    offset += sizeof(int);
    
    memcpy((char *)buf + offset, &b, sizeof(int));
    offset += sizeof(int);
    
    memcpy((char *)buf + offset, &c, sizeof(float));
    offset += sizeof(float);
}


// Prepare the tuple to right table in the format conforming to Insert/Update/ReadTuple, readAttribute
void prepareRightTuple(const int b, const float c, const int d, void *buf)
{
    int offset = 0;
    
    memcpy((char *)buf + offset, &b, sizeof(int));
    offset += sizeof(int);
    
    memcpy((char *)buf + offset, &c, sizeof(float));
    offset += sizeof(float);
    
    memcpy((char *)buf + offset, &d, sizeof(int));
    offset += sizeof(int);
}


void populateLeftTable(vector<RID> &rids)
{
    // Functions Tested
    // 1. InsertTuple
    RID rid;
    void *buf = malloc(bufsize);

    for(int i = 0; i < tuplecount; ++i)
    {
        memset(buf, 0, bufsize);
        
        // Prepare the tuple data for insertion
        // a in [0,99], b in [10, 109], c in [50, 149.0]
        int a = i;
        int b = i + 10;
        float c = (float)(i + 50);
        prepareLeftTuple(a, b, c, buf);
        
        RC rc = rm->insertTuple("left", buf, rid);
        assert(rc == success);
        rids.push_back(rid);
    }
    
    free(buf);
}


void populateRightTable(vector<RID> &rids)
{
    // Functions Tested
    // 1. InsertTuple
    RID rid;
    void *buf = malloc(bufsize);

    for(int i = 0; i < tuplecount; ++i)
    {
        memset(buf, 0, bufsize);
        
        // Prepare the tuple data for insertion
        // b in [20, 120], c in [25, 124.0], d in [0, 99]
        int b = i + 20;
        float c = (float)(i + 25);
        int d = i;
        prepareRightTuple(b, c, d, buf);
        
        RC rc = rm->insertTuple("right", buf, rid);
        assert(rc == success);
        rids.push_back(rid);
    }

    free(buf);
}


void createIndexforLeftB(vector<RID> &rids)
{
    RC rc;
    // Create Index
    rc = ixManager->CreateIndex("left", "B");
    assert(rc == success);
    
    // Open Index
    IX_IndexHandle ixHandle;
    rc = ixManager->OpenIndex("left", "B", ixHandle);
    assert(rc == success);
    
    // Insert Entry
    for(int i = 0; i < tuplecount; ++i)
    {
        // key in [10, 109]
        int key = i + 10;
              
        rc = ixHandle.InsertEntry(&key, rids[i]);
        assert(rc == success);
    }
    
    // Close Index
    rc = ixManager->CloseIndex(ixHandle);
    assert(rc == success);    
}


void createIndexforLeftC(vector<RID> &rids)
{
    RC rc;
    // Create Index
    rc = ixManager->CreateIndex("left", "C");
    assert(rc == success);
    
    // Open Index
    IX_IndexHandle ixHandle;
    rc = ixManager->OpenIndex("left", "C", ixHandle);
    assert(rc == success);
    
    // Insert Entry
    for(int i = 0; i < tuplecount; ++i)
    {
        // key in [50, 149.0]
        float key = (float)(i + 50);
        
        rc = ixHandle.InsertEntry(&key, rids[i]);
        assert(rc == success);
    }
    
    // Close Index
    rc = ixManager->CloseIndex(ixHandle);
    assert(rc == success);
}


void createIndexforRightB(vector<RID> &rids)
{
    RC rc;
    // Create Index
    rc = ixManager->CreateIndex("right", "B");
    assert(rc == success);
    
    // Open Index
    IX_IndexHandle ixHandle;
    rc = ixManager->OpenIndex("right", "B", ixHandle);
    assert(rc == success);
    
    // Insert Entry
    for(int i = 0; i < tuplecount; ++i)
    {
        // key in [20, 120]
        int key = i + 20;
              
        rc = ixHandle.InsertEntry(&key, rids[i]);
        assert(rc == success);
    }
    
    // Close Index
    rc = ixManager->CloseIndex(ixHandle);
    assert(rc == success);    
}


void createIndexforRightC(vector<RID> &rids)
{
    RC rc;
    // Create Index
    rc = ixManager->CreateIndex("right", "C");
    assert(rc == success);
    
    // Open Index
    IX_IndexHandle ixHandle;
    rc = ixManager->OpenIndex("right", "C", ixHandle);
    assert(rc == success);
    
    // Insert Entry
    for(int i = 0; i < tuplecount; ++i)
    {
        // key in [25, 124]
        float key = (float)(i + 25);
        
        // Insert the key into index
        rc = ixHandle.InsertEntry(&key, rids[i]);
        assert(rc == success);
    }
    
    // Close Index
    rc = ixManager->CloseIndex(ixHandle);
    assert(rc == success);
}


void testCase_1()
{
    // Functions Tested;
    // 1. Filter -- TableScan as input, on Integer Attribute
    cout << "****In Test Case 1****" << endl;
    
    TableScan *ts = new TableScan(*rm, "left");
    
    // Set up condition
    Condition cond;
    cond.lhsAttr = "left.B";
    cond.op = EQ_OP;
    cond.bRhsIsAttr = false;
    Value value;
    value.type = TypeInt;
    value.data = malloc(bufsize);
    *(int *)value.data = 149;
    cond.rhsValue = value;
    
    // Create Filter 
    Filter filter(ts, cond);
    
    // Go over the data through iterator
    void *data = malloc(bufsize);
    while(filter.getNextTuple(data) != QE_EOF)
    {
        int offset = 0;
        // Print left.A
        cout << "left.A " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        // Print left.B
        cout << "left.B " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        // Print left.C
        cout << "left.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);
        
        memset(data, 0, bufsize);
    }
   
    free(value.data); 
    free(data);
    cout << "****Pass Test Case 1****" << endl << endl;
    return;
}


void testCase_2()
{
    // Functions Tested
    // 1. Filter -- IndexScan as input, on TypeChar attribute
    cout << "****In Test Case 2****" << endl;
    
    IX_IndexHandle ixHandle;
    ixManager->OpenIndex("right", "C", ixHandle);
    IndexScan *is = new IndexScan(*rm, ixHandle, "right");
    
    // Set up condition
    Condition cond;
    cond.lhsAttr = "right.C";
    cond.op = EQ_OP;
    cond.bRhsIsAttr = false;
    Value value;
    value.type = TypeReal;
    value.data = malloc(bufsize);
    *(float *)value.data = 24.0;
    cond.rhsValue = value;
    
    // Create Filter
    is->setIterator(EQ_OP, value.data);
    Filter filter(is, cond);
    
    // Go over the data through iterator
    void *data = malloc(bufsize);
    while(filter.getNextTuple(data) != QE_EOF)
    {
        int offset = 0;
        // Print right.B
        cout << "right.B " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        // Print right.C
        cout << "right.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);
        
        // Print right.D
        cout << "right.D " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        memset(data, 0, bufsize);
    }

    ixManager->CloseIndex(ixHandle);
    free(value.data);
    free(data);
    cout << "****Pass Test Case 2****" << endl << endl;
    return;
}


void testCase_3()
{
    // Functions Tested
    // 1. Project -- TableScan as input  
    cout << "****In Test Case 3****" << endl;
    
    TableScan *ts = new TableScan(*rm, "right");
    
    vector<string> attrNames;
    attrNames.push_back("right.C");
    attrNames.push_back("right.D");
    
    // Create Projector 
    Project project(ts, attrNames);
    
    // Go over the data through iterator
    void *data = malloc(bufsize);
    unsigned count = 0;
    while(project.getNextTuple(data) != QE_EOF)
    {
        int offset = 0;

        // Print right.C
        cout << "right.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);

        // Print right.D
        cout << "right.D " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        count++;
        memset(data, 0, bufsize);
        cout << "===========================" << endl;
    }
    cout << "Total tuple number: " << count << endl;
    
    free(data);
    cout << "****Pass Test Case 3****" << endl << endl;
    return;
}


void testCase_4()
{
    // Functions Tested
    // 1. NLJoin -- on TypeInt Attribute
    cout << "****In Test Case 4****" << endl;
    
    // Prepare the iterator and condition
    TableScan *leftIn = new TableScan(*rm, "left");
    TableScan *rightIn = new TableScan(*rm, "right");
    
    Condition cond;
    cond.lhsAttr = "left.B";
    cond.op= EQ_OP;
    cond.bRhsIsAttr = true;
    cond.rhsAttr = "right.B";
    
    // Create NLJoin
    NLJoin nljoin(leftIn, rightIn, cond, 10);
        
    // Go over the data through iterator
    void *data = malloc(bufsize);
    unsigned count = 0;
    while(nljoin.getNextTuple(data) != QE_EOF)
    {
    	if (count % 50 == 0)
    	{
			int offset = 0;

			// Print left.A
			cout << "left.A " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.B
			cout << "left.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.C
			cout << "left.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.B
			cout << "right.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print right.C
			cout << "right.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.D
			cout << "right.D " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

	        cout << "=========================== count: " << count << endl;
    	}

        count++;
        memset(data, 0, bufsize);
    }
    cout << "Total tuple number: " << count << endl;
    
    free(data);
    cout << "****Pass Test Case 4****" << endl << endl;
    return;
}


void testCase_5()
{
    // Functions Tested
    // 1. INLJoin -- on TypeChar Attribute
    cout << "****In Test Case 5****" << endl;
    
    // Prepare the iterator and condition
    TableScan *leftIn = new TableScan(*rm, "left");
    
    IX_IndexHandle ixRightHandle;
    ixManager->OpenIndex("right", "C", ixRightHandle);
    IndexScan *rightIn = new IndexScan(*rm, ixRightHandle, "right");
    
    Condition cond;
    cond.lhsAttr = "left.C";
    cond.op = EQ_OP;
    cond.bRhsIsAttr = true;
    cond.rhsAttr = "right.C";
    
    // Create INLJoin
    INLJoin inljoin(leftIn, rightIn, cond, 10);
        
    // Go over the data through iterator
    void *data = malloc(bufsize);
    unsigned count = 0;
    while(inljoin.getNextTuple(data) != QE_EOF)
    {
    	if (count % 50 == 0)
    	{
			int offset = 0;

			// Print left.A
			cout << "left.A " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.B
			cout << "left.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.C
			cout << "left.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.B
			cout << "right.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print right.C
			cout << "right.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.D
			cout << "right.D " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

	        cout << "=========================== count: " << count << endl;
    	}

        count++;
        memset(data, 0, bufsize);
    }
    cout << "Total tuple number: " << count << endl;
   
    ixManager->CloseIndex(ixRightHandle); 
    free(data);
    cout << "****Pass Test Case 5****" << endl << endl;
    return;
}


void testCase_6()
{
    // Functions Tested
    // 1. HashJoin -- on TypeInt Attribute
    cout << "****In Test Case 6****" << endl;
    
    // Prepare the iterator and condition
    TableScan *leftIn = new TableScan(*rm, "left");
    TableScan *rightIn = new TableScan(*rm, "right");
    
    Condition cond;
    cond.lhsAttr = "left.B";
    cond.op = EQ_OP;
    cond.bRhsIsAttr = true;
    cond.rhsAttr = "right.B";
    
    // Create HashJoin
    HashJoin hashjoin(leftIn, rightIn, cond, 5);
        
    // Go over the data through iterator
    void *data = malloc(bufsize);
    unsigned count = 0;
    while(hashjoin.getNextTuple(data) != QE_EOF)
    {
    	if (count % 50 == 0)
    	{
			int offset = 0;

			// Print left.A
			cout << "left.A " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.B
			cout << "left.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.C
			cout << "left.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.B
			cout << "right.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print right.C
			cout << "right.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.D
			cout << "right.D " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

	        cout << "=========================== count: " << count << endl;
    	}

        count++;
        memset(data, 0, bufsize);
    }
    cout << "Total tuple number: " << count << endl;
   
    free(data);
    cout << "****Pass Test Case 6****" << endl << endl;
    return;
}


void testCase_7()
{
    // Functions Tested
    // 1. INLJoin -- on TypeInt Attribute
    // 2. Filter -- on TypeInt Attribute
    cout << "****In Test Case 7****" << endl;
    
    // Prepare the iterator and condition
    TableScan *leftIn = new TableScan(*rm, "left");
    
    IX_IndexHandle ixHandle;
    ixManager->OpenIndex("right", "B", ixHandle);
    IndexScan *rightIn = new IndexScan(*rm, ixHandle, "right");
    
    Condition cond_j;
    cond_j.lhsAttr = "left.B";
    cond_j.op = EQ_OP;
    cond_j.bRhsIsAttr = true;
    cond_j.rhsAttr = "right.B";
    
    // Create INLJoin
    INLJoin *inljoin = new INLJoin(leftIn, rightIn, cond_j, 5);
    
    // Create Filter
    Condition cond_f;
    cond_f.lhsAttr = "right.B";
    cond_f.op = EQ_OP;
    cond_f.bRhsIsAttr = false;
    Value value;
    value.type = TypeInt;
    value.data = malloc(bufsize);
    *(int *)value.data = 28;
    cond_f.rhsValue = value;
    
    Filter filter(inljoin, cond_f);
            
    // Go over the data through iterator
    void *data = malloc(bufsize);
    while(filter.getNextTuple(data) != QE_EOF)
    {
        int offset = 0;
 
        // Print left.A
        cout << "left.A " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        // Print left.B
        cout << "left.B " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
 
        // Print left.C
        cout << "left.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);
    
        // Print right.B
        cout << "right.B " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
 
        // Print right.C
        cout << "right.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);
         
        // Print right.D
        cout << "right.D " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        memset(data, 0, bufsize);
    }
   
    ixManager->CloseIndex(ixHandle); 
    free(value.data); 
    free(data);
    cout << "****Pass Test Case 7****" << endl << endl;
    return;
}


void testCase_8()
{
    // Functions Tested
    // 1. HashJoin -- on TypeChar Attribute
    // 2. Project
    cout << "****In Test Case 8****" << endl;
    
    // Prepare the iterator and condition
    TableScan *leftIn = new TableScan(*rm, "left");
    TableScan *rightIn = new TableScan(*rm, "right");
    
    Condition cond_j;
    cond_j.lhsAttr = "left.C";
    cond_j.op = EQ_OP;
    cond_j.bRhsIsAttr = true;
    cond_j.rhsAttr = "right.C";
    
    // Create HashJoin
    HashJoin *hashjoin = new HashJoin(leftIn, rightIn, cond_j, 10);
    
    // Create Projector
    vector<string> attrNames;
    attrNames.push_back("left.A");
    attrNames.push_back("right.D");
    
    Project project(hashjoin, attrNames);
        
    // Go over the data through iterator
    void *data = malloc(bufsize);
    unsigned count = 0;
    while(project.getNextTuple(data) != QE_EOF)
    {
    	if (count % 50 == 0)
    	{
			int offset = 0;

			// Print left.A
			cout << "left.A " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print right.D
			cout << "right.D " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

	        cout << "=========================== count: " << count << endl;
    	}

        count++;
        memset(data, 0, bufsize);
    }
    cout << "Total tuple number: " << count << endl;
   
    free(data);
    cout << "****Pass Test Case 8****" << endl << endl;
    return;
}


void testCase_9()
{
    // Functions Tested
    // 1. NLJoin -- on TypeChar Attribute
    // 2. HashJoin -- on TypeInt Attribute
    
    cout << "****In Test Case 9****" << endl;
    
    // Prepare the iterator and condition
    TableScan *leftIn = new TableScan(*rm, "left");
    TableScan *rightIn = new TableScan(*rm, "right");
    
    Condition cond;
    cond.lhsAttr = "left.C";
    cond.op = EQ_OP;
    cond.bRhsIsAttr = true;
    cond.rhsAttr = "right.C";
    
    // Create NLJoin
    NLJoin *nljoin = new NLJoin(leftIn, rightIn, cond, 10);
    
    // Create HashJoin
    TableScan *thirdIn = new TableScan(*rm, "left", "leftSecond");
    Condition cond_h;
    cond_h.lhsAttr = "left.B";
    cond_h.op = EQ_OP;
    cond_h.bRhsIsAttr = true;
    cond_h.rhsAttr = "leftSecond.B";
    HashJoin hashjoin(nljoin, thirdIn, cond_h, 8);

    // Go over the data through iterator
    void *data = malloc(bufsize);
    unsigned count = 0;
    while(hashjoin.getNextTuple(data) != QE_EOF)
    {
    	if (count % 50 == 0)
    	{
			int offset = 0;

			// Print left.A
			cout << "left.A " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.B
			cout << "left.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.C
			cout << "left.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.B
			cout << "right.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print right.C
			cout << "right.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.D
			cout << "right.D " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print leftSecond.A
			cout << "leftSecond.A " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.B
			cout << "leftSecond.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print leftSecond.C
			cout << "leftSecond.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

	        cout << "=========================== count: " << count << endl;
    	}

        count++;
        memset(data, 0, bufsize);
    }
    cout << "Total tuple number: " << count << endl;
   
    free(data);
    cout << "****Pass Test Case 9****" << endl << endl;
    return;
}


void testCase_10()
{
    // Functions Tested
    // 1. Filter  
    // 2. Project
    // 3. INLJoin
    
    cout << "****In Test Case 10****" << endl;

    // Create Filter
    IX_IndexHandle ixLeftHandle;
    ixManager->OpenIndex("left", "B", ixLeftHandle);
    IndexScan *leftIn = new IndexScan(*rm, ixLeftHandle, "left");

    Condition cond_f;
    cond_f.lhsAttr = "left.B";
    cond_f.op = EQ_OP;
    cond_f.bRhsIsAttr = false;
    Value value;
    value.type = TypeInt;
    value.data = malloc(bufsize);
    *(int *)value.data = 253;
    cond_f.rhsValue = value;
   
    leftIn->setIterator(EQ_OP, value.data); 
    Filter *filter = new Filter(leftIn, cond_f);

    // Create Projector
    vector<string> attrNames;
    attrNames.push_back("left.A");
    attrNames.push_back("left.C");
    Project *project = new Project(filter, attrNames);
    
    // Create INLJoin
    IX_IndexHandle ixRightHandle;
    ixManager->OpenIndex("right", "C", ixRightHandle);
    IndexScan *rightIn = new IndexScan(*rm, ixRightHandle, "right");

    Condition cond_j;
    cond_j.lhsAttr = "left.C";
    cond_j.op = EQ_OP;
    cond_j.bRhsIsAttr = true;
    cond_j.rhsAttr = "right.C";
    
    INLJoin inljoin(project, rightIn, cond_j, 8); 
    
    // Go over the data through iterator
    void *data = malloc(bufsize);
    while(inljoin.getNextTuple(data) != QE_EOF)
    {
        int offset = 0;
 
        // Print left.A
        cout << "left.A " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        // Print left.C
        cout << "left.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);
        
        // Print right.B
        cout << "right.B " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
 
        // Print right.C
        cout << "right.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);
        
        // Print right.D
        cout << "right.D " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);
        
        memset(data, 0, bufsize);
    }

    ixManager->CloseIndex(ixLeftHandle);
    ixManager->CloseIndex(ixRightHandle);
    free(value.data);
    free(data);
    cout << "****Pass Test Case 10****" << endl << endl;
    return;
}

void extraTest1()
{
    // Create Filters
    IX_IndexHandle ixLeftHandle1;
    ixManager->OpenIndex("left", "B", ixLeftHandle1);
    IndexScan *leftIn1 = new IndexScan(*rm, ixLeftHandle1, "left");

    Condition cond_f_plus;
    cond_f_plus.lhsAttr = "left.B";
    cond_f_plus.op = GE_OP;
    cond_f_plus.bRhsIsAttr = false;
    Value value1;
    value1.type = TypeInt;
    value1.data = malloc(bufsize);
    *(int *)value1.data = 500;
    cond_f_plus.rhsValue = value1;

    int key = 300;
    leftIn1->setIterator(GE_OP, &key);
    Filter *filter500plus = new Filter(leftIn1, cond_f_plus);
    cout << "==== Done Filter 1" << endl;

    IX_IndexHandle ixLeftHandle2;
    ixManager->OpenIndex("left", "B", ixLeftHandle2);
    IndexScan *leftIn2 = new IndexScan(*rm, ixLeftHandle2, "left");

    Condition cond_f_minus;
    cond_f_minus.lhsAttr = "left.B";
    cond_f_minus.op = LT_OP;
    cond_f_minus.bRhsIsAttr = false;
    Value value2;
    value2.type = TypeInt;
    value2.data = malloc(bufsize);
    *(int *)value2.data = 500;
    cond_f_minus.rhsValue = value2;

    key = 600;
    leftIn2->setIterator(LT_OP, &key);
    Filter *filter499minus = new Filter(leftIn2, cond_f_minus);
    cout << "==== Done Filter 2" << endl;

    // Create INLJoin
    IX_IndexHandle ixRightHandle;
    ixManager->OpenIndex("right", "C", ixRightHandle);
    IndexScan *ixRightIn = new IndexScan(*rm, ixRightHandle, "right");

    Condition cond_inlj;
    cond_inlj.lhsAttr = "left.C";
    cond_inlj.op = EQ_OP;
    cond_inlj.bRhsIsAttr = true;
    cond_inlj.rhsAttr = "right.C";

    INLJoin *inljoin = new INLJoin(filter500plus, ixRightIn, cond_inlj, 10);
    cout << "==== Done INLJoin." << endl;

    // Create NLJoin
    TableScan *rightIn = new TableScan(*rm, "right", "right2");

    Condition cond_nlj;
    cond_nlj.lhsAttr = "left.C";
    cond_nlj.op= EQ_OP;
    cond_nlj.bRhsIsAttr = true;
    cond_nlj.rhsAttr = "right2.C";

    NLJoin *nljoin = new NLJoin(filter499minus, rightIn, cond_nlj, 10);
    cout << "==== Done NLJoin." << endl;

    // Create Projector
    vector<string> attrNames;
    attrNames.push_back("left.B");
    attrNames.push_back("left.C");
    attrNames.push_back("right.C");
    Project *project1 = new Project(inljoin, attrNames);

    vector<string> attrNames2;
    attrNames2.push_back("left.B");
    attrNames2.push_back("left.C");
    attrNames2.push_back("right2.C");
    Project *project2 = new Project(nljoin, attrNames2);

    // Go over the data through iterator
    void *data = malloc(bufsize);
    unsigned count = 0;
    while(project1->getNextTuple(data) != QE_EOF)
    {
        int offset = 0;

        // Print left.B
        cout << "left.B " << *(int *)((char *)data + offset) << endl;
        offset += sizeof(int);

        // Print left.C
        cout << "left.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);

        // Print right.C
        cout << "right.C " << *(float *)((char *)data + offset) << endl;
        offset += sizeof(float);

        count++;
        memset(data, 0, bufsize);
        cout << "===========================" << endl;
    }
    cout << "Total tuple number: " << count << endl;
    free(data);

    data = malloc(bufsize);
    count = 0;
    while(project2->getNextTuple(data) != QE_EOF)
    {
    	if (count % 100 == 0)
    	{
			int offset = 0;

			// Print left.B
			cout << "left.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.C
			cout << "left.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.C
			cout << "right2.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);
	        cout << "===========================" << endl;
    	}

        count++;
        memset(data, 0, bufsize);
    }
    cout << "Total tuple number: " << count << endl;
    free(data);

    // final join
    Condition cond_fj;
    cond_fj.lhsAttr = "right.C";
    cond_fj.op= EQ_OP;
    cond_fj.bRhsIsAttr = true;
    cond_fj.rhsAttr = "right2.C";
    HashJoin hashjoin(project1, project2, cond_fj, 10);

    while(hashjoin.getNextTuple(data) != QE_EOF)
    {
    	if (count % 50 == 0)
    	{
			int offset = 0;

	        // Print left.B
	        cout << "left.B " << *(int *)((char *)data + offset) << endl;
	        offset += sizeof(int);

	        // Print left.C
	        cout << "left.C " << *(float *)((char *)data + offset) << endl;
	        offset += sizeof(float);

	        // Print right.C
	        cout << "right.C " << *(float *)((char *)data + offset) << endl;
	        offset += sizeof(float);

			// Print left.B
			cout << "left.B " << *(int *)((char *)data + offset) << endl;
			offset += sizeof(int);

			// Print left.C
			cout << "left.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

			// Print right.C
			cout << "right2.C " << *(float *)((char *)data + offset) << endl;
			offset += sizeof(float);

	        cout << "=========================== count: " << count << endl;
    	}

        count++;
        memset(data, 0, bufsize);
    }
    cout << "Total tuple number: " << count << endl;

    delete leftIn1;
    delete leftIn2;
    delete rightIn;
    delete ixRightIn;
    delete filter499minus;
    delete filter500plus;
    delete nljoin;
    delete inljoin;
    delete project1;
    delete project2;
}


//void extraTestCase_1()
//{
//    // Functions Tested
//    // 1. TableScan
//    // 2. Aggregate -- MAX
//    cout << "****In Extra Test Case 1****" << endl;
//
//    // Create TableScan
//    TableScan *input = new TableScan(*rm, "left");
//
//    // Create Aggregate
//    Attribute aggAttr;
//    aggAttr.name = "left.B";
//    aggAttr.type = TypeInt;
//    aggAttr.length = 4;
//    Aggregate agg(input, aggAttr, MAX);
//
//    void *data = malloc(bufsize);
//    while(agg.getNextTuple(data) != QE_EOF)
//    {
//        cout << "MAX(left.B) " << *(float *)data << endl;
//        memset(data, 0, sizeof(float));
//    }
//
//    free(data);
//    return;
//}
//
//
//void extraTestCase_2()
//{
//    // Functions Tested
//    // 1. TableScan
//    // 2. Aggregate -- AVG
//    cout << "****In Extra Test Case 2****" << endl;
//
//    // Create TableScan
//    TableScan *input = new TableScan(*rm, "right");
//
//    // Create Aggregate
//    Attribute aggAttr;
//    aggAttr.name = "right.B";
//    aggAttr.type = TypeInt;
//    aggAttr.length = 4;
//    Aggregate agg(input, aggAttr, AVG);
//
//    void *data = malloc(bufsize);
//    while(agg.getNextTuple(data) != QE_EOF)
//    {
//        cout << "AVG(right.B) " << *(float *)data << endl;
//        memset(data, 0, sizeof(float));
//    }
//
//    free(data);
//    return;
//}
//
//
//void extraTestCase_3()
//{
//    // Functions Tested
//    // 1. TableScan
//    // 2. Aggregate -- MIN
//    cout << "****In Extra Test Case 3****" << endl;
//
//    // Create TableScan
//    TableScan *input = new TableScan(*rm, "left");
//
//    // Create Aggregate
//    Attribute aggAttr;
//    aggAttr.name = "left.B";
//    aggAttr.type = TypeInt;
//    aggAttr.length = 4;
//
//    Attribute gAttr;
//    gAttr.name = "left.C";
//    gAttr.type = TypeReal;
//    gAttr.length = 4;
//    Aggregate agg(input, aggAttr, gAttr, MIN);
//
//    void *data = malloc(bufsize);
//    while(agg.getNextTuple(data) != QE_EOF)
//    {
//        int offset = 0;
//
//        // Print left.C
//        cout << "left.C " << *(float *)((char *)data + offset) << endl;
//        offset += sizeof(float);
//
//        // Print left.B
//        cout << "MIN(left.B) " << *(float *)((char *)data + offset) << endl;
//        offset += sizeof(int);
//
//        memset(data, 0, bufsize);
//    }
//
//    free(data);
//    return;
//}
//
//
//void extraTestCase_4()
//{
//    // Functions Tested
//    // 1. TableScan
//    // 2. Aggregate -- SUM
//    cout << "****In Extra Test Case 4****" << endl;
//
//    // Create TableScan
//    TableScan *input = new TableScan(*rm, "right");
//
//    // Create Aggregate
//    Attribute aggAttr;
//    aggAttr.name = "right.B";
//    aggAttr.type = TypeInt;
//    aggAttr.length = 4;
//
//    Attribute gAttr;
//    gAttr.name = "right.C";
//    gAttr.type = TypeReal;
//    gAttr.length = 4;
//    Aggregate agg(input, aggAttr, gAttr, SUM);
//
//    void *data = malloc(bufsize);
//    while(agg.getNextTuple(data) != QE_EOF)
//    {
//        int offset = 0;
//
//        // Print right.C
//        cout << "right.C " << *(float *)((char *)data + offset) << endl;
//        offset += sizeof(float);
//
//        // Print right.B
//        cout << "SUM(right.B) " << *(float *)((char *)data + offset) << endl;
//        offset += sizeof(int);
//
//        memset(data, 0, bufsize);
//    }
//
//    free(data);
//    return;
//}


int main() 
{
    // Create the left table, and populate the table
    vector<RID> leftRIDs;
    createLeftTable();
    populateLeftTable(leftRIDs);
    
    // Create the right table, and populate the table
    vector<RID> rightRIDs;
    createRightTable();
    populateRightTable(rightRIDs);
    
    // Create index for attribute B and C of the left table
    createIndexforLeftB(leftRIDs);
    createIndexforLeftC(leftRIDs);
    
    // Create index for attribute B and C of the right table
    createIndexforRightB(rightRIDs);
    createIndexforRightC(rightRIDs);
   
    // Test Cases

    // Filter -- TableScan as input, on Integer Attribute
    testCase_1();

    // Filter -- IndexScan as input, on TypeChar attribute
    testCase_2();

    // Project -- TableScan as input
    testCase_3();

    // NLJoin -- on TypeInt Attribute
    testCase_4();

    // INLJoin -- on TypeChar Attribute
    testCase_5();

    // HashJoin -- on TypeInt Attribute
    testCase_6();

    // 1. INLJoin -- on TypeInt Attribute
    // 2. Filter -- on TypeInt Attribute
    testCase_7();

    // 1. HashJoin -- on TypeChar Attribute
    // 2. Project
    testCase_8();

    // 1. NLJoin -- on TypeChar Attribute
    // 2. HashJoin -- on TypeInt Attribute
    testCase_9();

    // 1. Filter
    // 2. Project
    // 3. INLJoin
    testCase_10();

//    // Extra Credit
//    extraTestCase_1();
//    extraTestCase_2();
//    extraTestCase_3();
//    extraTestCase_4();

//    extraTest1();

    return 0;
}

