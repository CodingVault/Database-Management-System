
#include <fstream>
#include <iostream>
#include <cassert>

#include "qe.h"

using namespace std;

RM *rm = RM::Instance();

// Function to prepare the data in the correct form to be inserted/read/updated
void prepareEmpTuple(const int name_length, const string name, const int dno, const int age, const float height, const int salary, void *buffer, int *tuple_size)
{
    int offset = 0;

    memcpy((char *)buffer + offset, &name_length, sizeof(int));
    offset += sizeof(int);
    memcpy((char *)buffer + offset, name.c_str(), name_length);
    offset += name_length;

    memcpy((char *)buffer + offset, &dno, sizeof(int));
    offset += sizeof(int);

    memcpy((char *)buffer + offset, &age, sizeof(int));
    offset += sizeof(int);

    memcpy((char *)buffer + offset, &height, sizeof(float));
    offset += sizeof(float);

    memcpy((char *)buffer + offset, &salary, sizeof(int));
    offset += sizeof(int);

    *tuple_size = offset;
}

// Function to parse the data in buffer and print each field
void printEmpTuple(const void *buffer)
{
    int offset = 0;
    cout << "****Printing Employee Buffer: Start****" << endl;

    int name_length = 0;
    memcpy(&name_length, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "name_length: " << name_length << endl;

    char *name = (char *)malloc(100);
    memcpy(name, (char *)buffer+offset, name_length);
    name[name_length] = '\0';
    offset += name_length;
    cout << "name: " << name << endl;

    int dno = 0;
    memcpy(&dno, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "dno: " << dno << endl;

    int age = 0;
    memcpy(&age, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "age: " << age << endl;

    float height = 0.0;
    memcpy(&height, (char *)buffer+offset, sizeof(float));
    offset += sizeof(float);
    cout << "height: " << height << endl;

    int salary = 0;
    memcpy(&salary, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "salary: " << salary << endl;

    cout << "****Printing Employee Buffer: End****" << endl << endl;
}

// Create an employee table
void createEmpTable(const string tablename)
{
    cout << "****Create Table " << tablename << " ****" << endl;

    // 1. Create Table ** -- made separate now.
    vector<Attribute> attrs;

    Attribute attr;
    attr.name = "EmpName";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)30;
    attrs.push_back(attr);

    attr.name = "DeptNo";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    attr.name = "Age";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    attr.name = "Height";
    attr.type = TypeReal;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    attr.name = "Salary";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    RC rc = rm->createTable(tablename, attrs);
    assert(rc == SUCCESS);
    cout << "****Table Created: " << tablename << " ****" << endl << endl;
}

// Function to prepare the data in the correct form to be inserted/read/updated
void prepareDeptTuple(const int name_length, const string name, const int dno, void *buffer, int *tuple_size)
{
    int offset = 0;

    memcpy((char *)buffer + offset, &name_length, sizeof(int));
    offset += sizeof(int);
    memcpy((char *)buffer + offset, name.c_str(), name_length);
    offset += name_length;

    memcpy((char *)buffer + offset, &dno, sizeof(int));
    offset += sizeof(int);

    *tuple_size = offset;
}

// Function to parse the data in buffer and print each field
void printDeptTuple(const void *buffer)
{
    int offset = 0;
    cout << "****Printing Department Buffer: Start****" << endl;

    int name_length = 0;
    memcpy(&name_length, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "name_length: " << name_length << endl;

    char *name = (char *)malloc(100);
    memcpy(name, (char *)buffer+offset, name_length);
    name[name_length] = '\0';
    offset += name_length;
    cout << "name: " << name << endl;

    int dno = 0;
    memcpy(&dno, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "dno: " << dno << endl;

    cout << "****Printing Department Buffer: End****" << endl << endl;
}

// Create an department table
void createDeptTable(const string tablename)
{
    cout << "****Create Table " << tablename << " ****" << endl;

    // 1. Create Table ** -- made separate now.
    vector<Attribute> attrs;

    Attribute attr;
    attr.name = "DeptName";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)30;
    attrs.push_back(attr);

    attr.name = "DeptNo";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    RC rc = rm->createTable(tablename, attrs);
    assert(rc == SUCCESS);
    cout << "****Table Created: " << tablename << " ****" << endl << endl;
}

void insertEmp(const string tablename, const int name_length, const string name, const int dno, const int age, const float height, const int salary)
{
    RID rid;
    int tuple_size = 0;
    void *tuple = malloc(100);

    // Insert a tuple into a table
    prepareEmpTuple(name_length, name, dno, age, height, salary, tuple, &tuple_size);
    cout << "Insert Employee Data:" << endl;
    printEmpTuple(tuple);
    RC rc = rm->insertTuple(tablename, tuple, rid);
    assert(rc == SUCCESS);

    free(tuple);
    return;
}

void insertDept(const string tablename, const int name_length, const string name, const int dno)
{
    RID rid;
    int tuple_size = 0;
    void *tuple = malloc(100);

    // Insert a tuple into a table
    prepareDeptTuple(name_length, name, dno, tuple, &tuple_size);
    cout << "Insert Department Data:" << endl;
    printDeptTuple(tuple);
    RC rc = rm->insertTuple(tablename, tuple, rid);
    assert(rc == SUCCESS);

    free(tuple);
    return;
}

void printAttribute(const vector<Attribute> &attrs)
{
	cout << "****Printing attributes begin****" << endl;
	for (unsigned i = 0; i < attrs.size(); ++i)
		cout << "Attribute " << i << " - name: " << attrs[i].name << "; type: " << attrs[i].type << endl;
	cout << "****Printing attributes begin****" << endl;
}

void printTuple(void *data, const vector<Attribute> &attrs)
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
			char *temp = (char *)malloc(iValue);
			memcpy(temp, (char *)data + offset, iValue);
			offset += iValue;
			temp[iValue] = '\0';
			cout << attrs[i].name << ": " << temp << endl;
			break;
		}
	}
	cout << "****Printing tuple end****" << endl << endl;
}

void opTest()
{
	string empTableName = "Employee";
	createEmpTable(empTableName);
	insertEmp(empTableName, 4, "John", 80, 20, 182.5, 3000);
	insertEmp(empTableName, 6, "Peters", 100, 24, 170.1, 5000);
	insertEmp(empTableName, 5, "Marry", 120, 22, 165.8, 8000);

	string deptTableName = "Department";
	createDeptTable(deptTableName);
	insertDept(deptTableName, 3, "Toy", 100);

	TableScan *empScan = new TableScan(*rm, empTableName);
	TableScan *deptScan = new TableScan(*rm, deptTableName);
	void *data = malloc(BUFF_SIZE);
	vector<Attribute> attrs;

	Condition cond_f;
	cond_f.lhsAttr = empTableName + ".Salary";
	cond_f.op = GT_OP;
	cond_f.bRhsIsAttr = false;
	Value value;
	value.type = TypeInt;
	value.data = malloc(BUFF_SIZE);
	*(int *)value.data = 5000;
	cond_f.rhsValue = value;

//	Filter *filter = new Filter(empScan, cond_f);

	vector<string> attrNames;
	attrNames.push_back(empTableName + ".EmpName");
	attrNames.push_back(empTableName + ".Age");
	attrNames.push_back(empTableName + ".DeptNo");

	cout << "******* Test Project Begin *******" << endl;
	Project project(empScan, attrNames);
	project.getAttributes(attrs);
	while (project.getNextTuple(data) != QE_EOF)
		printTuple(data, attrs);
	cout << "******* Test Project End *******" << endl << endl;

	Condition cond_j;
	cond_j.lhsAttr = empTableName + ".DeptNo";
	cond_j.op = EQ_OP;
	cond_j.bRhsIsAttr = true;
	cond_j.rhsAttr = deptTableName + ".DeptNo";

	empScan->setIterator();
	deptScan->setIterator();
	cout << "******* Test NLJoin Begin *******" << endl;
	NLJoin *nlJoin  = new NLJoin(empScan, deptScan, cond_j, 10000);
	nlJoin->getAttributes(attrs);
	printAttribute(attrs);
	while (nlJoin->getNextTuple(data) != QE_EOF)
		printTuple(data, attrs);
	cout << "******* Test NLJoin End *******" << endl << endl;

	free(data);
	free(value.data);
	delete empScan;
	delete deptScan;
}

int main() 
{
	cout << "test..." << endl;

	opTest();

	cout << "PASS..." << endl;
}
