
#include <fstream>
#include <iostream>
#include <cassert>
#include <set>

#include "ix.h"

using namespace std;
const int success = 0;
IX_Manager *ix_manager = IX_Manager::Instance();

/*
* get_shuffle --
* Construct a random shuffle array of t elements
*/
/*
* This algorithm is taken from D. E. Knuth,
* The Art of Computer Programming, Volume 2:
* Seminumerical Algorithms, 2nd Ed., page 139.
*/

static unsigned* get_shuffle(unsigned t)
{
    unsigned *shuffle;
    unsigned i, j, k, temp;
    shuffle = (unsigned*)malloc(t * sizeof(unsigned));
    for (i = 0; i < t; i++)
        shuffle[i] = i;

    for (j = t - 1; j > 0; j--)
    {
    	k = rand() % (j + 1);
        temp = shuffle[j];
        shuffle[j] = shuffle[k];
        shuffle[k] = temp;
     }

     return shuffle;
}

void createTable(RM *rm, const string tablename)
{
    // Functions tested
    // 1. Create Table
    vector<Attribute> attrs;

    Attribute attr;
    attr.name = "EmpName";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)100;
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
    assert(rc == success);
    cout << "****Table Created: " << tablename << " ****" << endl << endl;
}

RC insertEntry(IX_IndexHandle &handle, unsigned key, const unsigned pageNum, const unsigned slotNum)
{
	RID rid;
	rid.pageNum = pageNum;
	rid.slotNum = slotNum;
	//cout << "==========>>> TEST: inserting (" << key << ") -> [" << pageNum << ":" << slotNum << "]." << endl;
	return handle.InsertEntry(&key, rid);
}

/*
 *  the test for the cross running of insertion and deletion operation
 *  maxTupleNum : the maximal value of the key
 *  round: round of test
 */
void ixTest2(unsigned round,unsigned maxTupleNum)
{
	unsigned  i = 0, j = 0;
	RID rid;
	RC rc;
	IX_IndexHandle ix_handle;
	set<int> btree;        // use set  to track the insertion and deletion,
	                       //can  be used to check correctness of each operation
	set<int>::iterator btree_i;
	rc = ix_manager->CreateIndex("tbl_employee", "Age");
	assert(rc == SUCCESS);
	rc = ix_manager->OpenIndex("tbl_employee", "Age", ix_handle);
	assert(rc == SUCCESS);

	//initial insertion from 0 to maxTupleNum
	for( i = 0; i < maxTupleNum; i++ )
	{
		rc = insertEntry(ix_handle, i, i,i);
		btree.insert(i);
		assert(rc == SUCCESS);
	}
	rc = ix_manager->CloseIndex(ix_handle);
	assert(rc == SUCCESS);
	cout << endl << "Inserted " << maxTupleNum << " entries..." << endl << endl << endl;

	//unsigned* P = NULL;
	unsigned tempNum1 = 0;
	unsigned tempNum2 = 0;
	unsigned opNum = 0;
	for(i = 0; i < round; i++)
	{
		cout<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
		cout<<"==================round "<<i<<endl;
		rc = ix_manager->OpenIndex("tbl_employee", "Age", ix_handle);
		assert(rc == SUCCESS);
		tempNum1 = rand()%20 + 10; // the number of operation in each round
		opNum = rand()%2;          // 0: do insertion for the current round
		                           //1 : do deletion for the current round
		for(j = 0; j < tempNum1; j++)
		{
			tempNum2 = rand()%(maxTupleNum); // randomly generate a key
			btree_i = btree.find(tempNum2);
			if(opNum == 0)
			{//insert
				cout<<"---------------------------"<<endl;
				cout<<"want to insert "<<tempNum2<<endl;
				rc = insertEntry(ix_handle, tempNum2, tempNum2,tempNum2);
				if(btree_i != btree.end()) // the key is already in the tree, insert should fail
				{
					cout<<"can not insert "<<tempNum2<<endl;
					assert( rc != success );
				}
				else                      //insert should succeed
				{
					btree.insert(tempNum2);
					assert( rc == success );
					cout<<"insert "<<tempNum2<<" successfully"<<endl;
				}
				cout<<"---------------------------"<<endl;
			}
			else
			{//delete
				cout<<"---------------------------"<<endl;
				cout<<"want to delete "<<tempNum2<<endl;
				rid.pageNum = tempNum2;
				rid.slotNum = tempNum2;
				rc = ix_handle.DeleteEntry(&tempNum2, rid);
				cout << "DONE" << endl;
				if(btree_i != btree.end()) // the key is in the tree, deletion should succeed
				{
					btree.erase(tempNum2);
					assert( rc == success );
					cout<<"delete "<<tempNum2<<" successfully"<<endl;
				}
				else                      //deletion should fail
				{
					cout<<"can not delete "<<tempNum2<<endl;
					assert( rc != success );
				}
				cout<<"---------------------------"<<endl;
			}
		}
		cout<<"==================round ["<<i<<"] OK!"<<endl;
		cout<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl<<endl;
		rc = ix_manager->CloseIndex(ix_handle);
		assert(rc == SUCCESS);
	}

}

void ixTest1(unsigned tupleNum)
{
	unsigned  i = 0;
	RID rid;
	IX_IndexHandle ix_handle;
	RC rc = ix_manager->CreateIndex("tbl_employee", "Age");
	assert(rc == SUCCESS);
	rc = ix_manager->OpenIndex("tbl_employee", "Age", ix_handle);
	assert(rc == SUCCESS);
	//cout << "==== Open index done!" << endl << endl;
	unsigned* P = get_shuffle(tupleNum);
	for( i = 0; i < tupleNum; i++ )
	{
		//cout<<"inserted key: "<<P[i]<<endl;
		rc = insertEntry(ix_handle, P[i], P[i],P[i]);
		//rc = insertEntry(ix_handle, i, i,i);
		assert(rc == SUCCESS);
	}

	cout << "==== Insert entry done!" << endl << endl;
	rc = ix_manager->CloseIndex(ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Close index done!" << endl << endl;
	rc = ix_manager->OpenIndex("tbl_employee", "Age", ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Open index done!" << endl << endl;
	cout << "==== Delete entry begins!" << endl << endl;
	delete P;
	P = get_shuffle(tupleNum);
	for(i = 0; i < tupleNum; i++)
	{
		//cout<<"deleted key: "<<P[i]<<endl;
		rid.slotNum  = P[i];
		rid.pageNum = P[i];
		rc = ix_handle.DeleteEntry(&P[i],rid);
		assert(rc == SUCCESS);
		//cout<<" there are "<<tupleNum - 1 - i<<" nodes remained in the B Tree"<<endl;
		//cout<<"============================="<<endl;
	}

	cout << "==== Delete entry done!" << endl << endl;
	cout << "==== Insert entry again begins!" << endl << endl;
    free(P);
    P = get_shuffle(tupleNum);
	for(i = 0; i < tupleNum; i++)
    {
		rid.slotNum  = P[i];
		rid.pageNum = P[i];
		rc = ix_handle.InsertEntry(&P[i],rid);
		assert(rc == SUCCESS);
		//cout<<"inserted key: "<<P[i]<<endl;
		//cout<<"============================="<<endl;
	}
	cout<< "==== Insert entry again done!" << endl << endl;
	rc = ix_manager->CloseIndex(ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Close index done!" << endl << endl;
    cout << "==== Delete entry again begins!" << endl << endl;
	rc = ix_manager->OpenIndex("tbl_employee", "Age", ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Open index done!" << endl << endl;
	P = get_shuffle(tupleNum);
	for(i = 0; i < tupleNum; i++)
	{
		//cout<<"deleted key: "<<P[i]<<endl;
		rid.slotNum  = P[i];
		rid.pageNum = P[i];
		rc = ix_handle.DeleteEntry(&P[i],rid);
		assert(rc == SUCCESS);
		//cout<<"============================="<<endl;
	}
	free(P);
	cout << "==== Delete entry again done!" << endl << endl;
	rc = ix_manager->CloseIndex(ix_handle);
	assert(rc == SUCCESS);
	cout << "==== Close index done!" << endl << endl;
	rc = ix_manager->DestroyIndex("tbl_employee", "Age");
	assert(rc == SUCCESS);
	cout << "==== Destroy index done!" << endl << endl;
	cout<<"the "<<tupleNum<<"th test OK!"<<endl;
}

int main()
{
  cout << "test..." << endl;

  RM *rm = RM::Instance();
  createTable(rm, "tbl_employee");

//  ixTest1(10000);
  ixTest2(100, 10000);


  cout << "OK" << endl;
}
