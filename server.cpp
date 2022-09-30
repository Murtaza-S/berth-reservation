#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <string.h>
#include <sstream>
#define berth_count 10
#define train_count 3
#define limit 1000
using namespace std;
#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

struct reservation
{
    int pnr;
    char pass_name[20];
    char sex[1];
    int age;
    char class_t[4]; // AC2, AC3, or SC
    int status;      // waitlisted
};
struct train
{
    int train_id;
    int AC2, AC3, SC;
    struct reservation rlist[limit];
};
struct train *rail_info;

int stringTointeger(string str)
{
    int temp = 0;
    for (int i = 0; i < str.length(); i++)
    {
        temp = temp * 10 + (str[i] - '0');
    }
    return temp;
}

int *reader_c_arr[train_count];
int shmid, shmid1, shmid2;
struct sembuf pop, vop;
int read_Sema[train_count];
int ex_Sema[train_count];
int write_Sema[train_count];

int *count_res;

void getWriteLock(int train_id)
{
    int i = train_id;
    P(write_Sema[i]);
    P(ex_Sema[i]);
}

void releaseWriteLock(int train_id)
{
    int i = train_id;
    V(ex_Sema[i]);
    V(write_Sema[i]);
}

void getReadLock(int train_id)
{
    int i = train_id;
    P(read_Sema[i]);
    int v;
    v = semctl(write_Sema[i], 0, GETVAL);
    while (v < 0)
        ;
    ++(*reader_c_arr[i]);
    if (*reader_c_arr[i] == 1)
    {
        P(ex_Sema[i]);
    }
    V(read_Sema[i]);
}

void releaseReadLock(int train_id)
{
    int i = train_id;
    P(read_Sema[i]);
    --(*reader_c_arr[i]);
    if ((*reader_c_arr[i]) == 0)
    {
        V(ex_Sema[i]);
    }
    V(read_Sema[i]);
}

int main(int argc, char *argv[])
{
    shmid = shmget(IPC_PRIVATE, 4 * sizeof(int), 0777 | IPC_CREAT);
    shmid1 = shmget(IPC_PRIVATE, 3 * sizeof(train), 0777 | IPC_CREAT);
    shmid2 = shmget(IPC_PRIVATE, sizeof(int), 0777 | IPC_CREAT);

    rail_info = (struct train *)shmat(shmid1, 0, 0);
    count_res = (int *)shmat(shmid2, 0, 0);
    count_res[0] = 0;

    for (int i = 0; i < train_count; i++)
    {
        rail_info[i].train_id = i;
        rail_info[i].AC2 = berth_count;
        rail_info[i].AC3 = berth_count;
        rail_info[i].SC = berth_count;
        for (int j = 0; j < limit; j++)
        {
            rail_info[i].rlist[j].age = -1;
            rail_info[i].rlist[j].pnr = -1;
            rail_info[i].rlist[j].status = -1;
        }
    }

    for (int i = 0; i < train_count; i++)
    {

        read_Sema[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
        write_Sema[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);
        ex_Sema[i] = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT);

        semctl(read_Sema[i], 0, SETVAL, 1);
        semctl(write_Sema[i], 0, SETVAL, 1);
        semctl(ex_Sema[i], 0, SETVAL, 1);
    }
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = SEM_UNDO;
    pop.sem_op = -1;
    vop.sem_op = 1;

    int process_currid, i;
    for (i = 0; i < 4; i++)
    {
        process_currid = fork();
        if (process_currid == 0)
        {
            break;
        }
        else if (process_currid == -1)
        {
            cout << "Error while creating process" << endl;
        }
    }

    string fn;
    string s;
    ifstream infile;
    int process_id = getpid();
    //getline(infile, s, '\n');
    if (i == 0)
    {
        cout << " I am process 0 and my process_id is " << process_id << endl;
        fn = argv[1];
    }
    else if (i == 1)
    {
        cout << "I am process 1 and my process_id is " << process_id << endl;
        fn = argv[2];
    }
    else if (i == 2)
    {
        cout << "I am process 2 and my process_id is " << process_id << endl;
        fn = argv[3];
    }
    else if (i == 3)
    {
        cout << "I am process 3 and my process_id is " << process_id << endl;
        fn = argv[4];
    }
    else
    {
        // cout << "I am parent process and my process_id is waiting for everyone to be executed " << process_id << endl;
        while (wait(NULL) > 0)
            ;
        cout << "All Processes exited " << endl;
        shmdt(rail_info);
        shmdt(count_res);

        shmctl(shmid, IPC_RMID, NULL);
        shmctl(shmid1, IPC_RMID, NULL);
        shmctl(shmid2, IPC_RMID, NULL);
        cout << "I am parent  and I am exiting my process id is: " << process_id << endl;
        return 0;
    }

    infile.open(fn);
    //cout<<"file is opened "<<endl;
    int num = 0;
    while (!infile.eof())
    {
        string str, string_read;
        string parameters[7];
        int k = 0;
        getline(infile, str);
        stringstream ss(str);
        while (ss >> string_read)
        {
            parameters[k] = string_read;
            k++;
        }

        if (parameters[0] == "reserve")
        {
            int train_n = stringTointeger(parameters[5]);
            string name = parameters[1] + parameters[2];
            getWriteLock(train_n);
            rail_info[train_n].train_id = train_n;
            rail_info[train_n].rlist[count_res[0]].pnr = count_res[0] * 10 + train_n;
            rail_info[train_n].rlist[count_res[0]].age = stringTointeger(parameters[3]);
            strcpy(rail_info[train_n].rlist[count_res[0]].pass_name, name.c_str());
            strcpy(rail_info[train_n].rlist[count_res[0]].class_t, parameters[6].c_str());
            strcpy(rail_info[train_n].rlist[count_res[0]].sex, parameters[4].c_str());

            if (parameters[6].compare("AC2") == 0)
            {
                if (rail_info[train_n].AC2 > 0)
                {
                    rail_info[train_n].rlist[count_res[0]].status = 0;
                    rail_info[train_n].AC2--;
                    //cout << "Entered into booking" << endl;
                    cout << rail_info[train_n].rlist[count_res[0]].pass_name << "Your booking is confirmed, details are: " << rail_info[train_n].train_id << " Class : " << rail_info[train_n].rlist[count_res[0]].class_t << " Seat Number: " << rail_info[train_n].AC2 << " PNR: " << rail_info[train_n].rlist[count_res[0]].pnr << endl;
                }
                else
                {

                    rail_info[train_n].rlist[count_res[0]].status = 1;
                    cout << rail_info[train_n].rlist[count_res[0]].pass_name << "You are added to waiting list and details are as follows: " << rail_info[train_n].train_id << " Class : " << rail_info[train_n].rlist[count_res[0]].class_t << " PNR: " << rail_info[train_n].rlist[count_res[0]].pnr << endl;
                }
            }

            else if (parameters[6].compare("AC3") == 0)
            {
                if (rail_info[train_n].AC3 > 0)
                {
                    rail_info[train_n].rlist[count_res[0]].status = 0;
                    rail_info[train_n].AC3--;
                    cout << rail_info[train_n].rlist[count_res[0]].pass_name << "Your booking is confirmed, details are: " << rail_info[train_n].train_id << " Class : " << rail_info[train_n].rlist[count_res[0]].class_t << " Seat Number: " << rail_info[train_n].AC3 << " PNR: " << rail_info[train_n].rlist[count_res[0]].pnr << endl;
                }
                else
                {
                    rail_info[train_n].rlist[count_res[0]].status = 1;
                    cout << rail_info[train_n].rlist[count_res[0]].pass_name << "You are added to waiting list and details are as follows: " << rail_info[train_n].train_id << " Class : " << rail_info[train_n].rlist[count_res[0]].class_t << " PNR: " << rail_info[train_n].rlist[count_res[0]].pnr << endl;
                }
            }

            else if (parameters[6].compare("SC") == 0)
            {
                if (rail_info[train_n].SC > 0)
                {
                    rail_info[train_n].rlist[count_res[0]].status = 0;
                    rail_info[train_n].SC--;
                    cout << rail_info[train_n].rlist[count_res[0]].pass_name << "Your booking is confirmed, details are: " << rail_info[train_n].train_id << " Class : " << rail_info[train_n].rlist[count_res[0]].class_t << " Seat Number: " << rail_info[train_n].AC3 << " PNR: " << rail_info[train_n].rlist[count_res[0]].pnr << endl;
                }
                else
                {
                    rail_info[train_n].rlist[count_res[0]].status = 1;
                    cout << rail_info[train_n].rlist[count_res[0]].pass_name << "You are added to waiting list and details are as follows: " << rail_info[train_n].train_id << " Class : " << rail_info[train_n].rlist[count_res[0]].class_t << " PNR: " << rail_info[train_n].rlist[count_res[0]].pnr << endl;
                }
            }

            count_res[0]++;
            releaseWriteLock(train_n);
        }
        else
        {
            int pnr = stringTointeger(parameters[1]);
            int train_n = pnr % 10;
            int ind = pnr / 10;
            char class_t[4] = {};
            getWriteLock(train_n);
            bool check = false;
            if (rail_info[train_n].rlist[ind].pnr == pnr)
            {
                rail_info[train_n].rlist[ind].status = 2;
                if (strcmp("AC3", rail_info[train_n].rlist[ind].class_t) == 0)
                {
                    if (rail_info[train_n].AC3 == 0)
                        check = true;

                    rail_info[train_n].AC3++;
                }
                else if (strcmp("AC2", rail_info[train_n].rlist[ind].class_t) == 0)
                {
                    if (rail_info[train_n].AC2 == 0)
                        check = true;

                    rail_info[train_n].AC2++;
                }
                else
                {
                    if (rail_info[train_n].SC == 0)
                        check = true;

                    rail_info[train_n].SC++;
                }
                cout << "Your reservation has been Cancelled " << rail_info[train_n].rlist[ind].pass_name << " " << check << endl;
                if (check)
                {

                    for (int m = ind; m < limit; m++)
                    {
                        string cclass(rail_info[train_n].rlist[ind].class_t);
                        string wclass(rail_info[train_n].rlist[ind].class_t);
                        int status = rail_info[train_n].rlist[m].status;
                        if ((cclass == wclass) && status == 1)
                        {
                            rail_info[train_n].rlist[m].status = 0;
                            cout << rail_info[train_n].rlist[m].pass_name << " Your waiting has been cleared " << train_n << " PNR: " << rail_info[train_n].rlist[m].pnr << endl;
                            break;
                        }
                    }
                }
            }
            else
            {
                cout << "PNR is invalid " << endl;
            }
            releaseWriteLock(train_n);
        }
    }
    infile.close();
    cout << "I am exiting and my process_id is: " << process_id << endl;
    return 0;
}