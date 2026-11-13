// ======================= TIME-TRAVEL DEBUGGER - CLIENT TEMPLATE =======================

// The client never talks to the server directly. It only talks to the load balancer.
// The load balancer wakes the server up if it is down, so the client does not care.
// The first "start" may just take a bit longer.
//
// The network is used ONLY by "start". After the .tdbg file is downloaded,
// next / prv / jmp read that file on the local disk. They never contact the load balancer.
//
// How the pieces fit together:
//   main loop -> readCommand -> runCommand
//   start -> cmdStart()  -> requestDebugFile() (Part A) -> openTdbg() (Part B) -> shows step 1
//   next, prv, jmp -> moveBy() (Part D) -> readSnapshotAt() (Part B) -> printSnapshot() (Part E)

#include <iostream>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <vector>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "helper.h"
using namespace std;

// ---- Constants ----
const char* PROGRAM_NAME = "ttdb";
const char* PROMPT = "ttdb> ";
const int IO_BUFFER_SIZE = 64 * 1024;     // fixed buffer for streaming files to and from the socket
const char* TDBG_FILE = "ttdb_last.tdbg"; // where the downloaded .tdbg is saved
const int MAX_JUMP = 1000000000;          // biggest n accepted by jmp
string LB_HOST = "127.0.0.1";
int LB_PORT = 5000;

// These two must be the same as in Phase 1


// ---- Structs (same as Phase 1, so the file can be read back exactly as it was written) ----

// ---- Structs only the client needs ----
struct TdbgReader
{
    ssize_t file_index;
    TTDBHeader header;
    vector<int64_t> index;
};
struct DebugSession
{
    int active_flag;     // false until a "start" has worked
    TdbgReader reader;   // the .tdbg file stays open for the whole session
    int64_t currentStep;
};

// =============================================================================================
// PART A: TALKING TO THE LOAD BALANCER
// =============================================================================================



int connectToHost(const char* host, int port)
{
    
    // Create a TCP socket and connect it to host:port.
    // Return the socket, or -1 if the connection fails.
      int soc=socket(AF_INET,SOCK_STREAM | SOCK_CLOEXEC,0);
    if(soc<0)
    {
        return -1;

    }
  


    sockaddr_in addr={};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    if(inet_pton(AF_INET,host,&addr.sin_addr)!=1)
    {
       close(soc);
       return -1;
    }
   if(connect(soc,(sockaddr*)&addr,sizeof(addr))<0)
    {
       close(soc);
       return -1;
    }
    return soc;
}
bool uploadSource(int sock, const char* sourcePath)
{
    FILE* f=fopen(sourcePath,"rb");
    if(f==nullptr)
{
    return false;
}
char status=0;
Crc crc;
crc.addBytes(&status,1);

if(!sendAll(sock,&status,1))
{
    fclose(f);
    return false;
}


char buffer[IO_BUFFER_SIZE];
size_t got;
while((got=fread(buffer,1,IO_BUFFER_SIZE,f))>0)
{
      crc.addBytes(buffer, got);
        if (!sendAll(sock, buffer, got))
        {
            fclose(f);
            return false;
        }
}
fclose(f);

char trailer[4];
numberToFourBytes(crc.finish(),trailer);
 if (!sendAll(sock, trailer, 4))
        {
        
            return false;
        }
 shutdown(sock,SHUT_WR);
 return true;       
    // have to do checksum and then send
    // When the file is finished, call shutdown(sock, SHUT_WR). This is how the server knows the string is complete.
    // Return false if the file cannot be opened or any send fails.
}
bool receiveResponse(int sock, const char* outTdbgPath, string& outError)
{
    Trailchecker checker;
   char buffer[IO_BUFFER_SIZE];
   bool gotstatus=false;
   char status=0;
   FILE* out =nullptr;
   outError="";
   while(true)
   {
    ssize_t r=recv(sock,buffer,IO_BUFFER_SIZE,0);
    if(r<0)
    {
        if(out)
        {
            fclose(out);
            remove(outTdbgPath);

        }
        outError="connections err";
        return false;
    }


    if (r == 0)
    {
            break;  
    }                            

       
        string released;
        checker.feed(buffer, r, released);
        if (released.empty())
        {
            continue;    
        }                       

        size_t startAt = 0;
        if (!gotstatus)
        {
            status = released[0];               
            gotstatus = true;
            startAt = 1;
            if (status == 0)
            {
                out = fopen(outTdbgPath, "wb");
                if (out == nullptr)
                {
                    outError = "cannot create file";
                    {
                    return false;
                    }
                }
            }
        }

        if (status == 1)
        {
            outError.append(released, startAt, string::npos); 
        }         
        else if (out != nullptr)
        {
            fwrite(released.data() + startAt, 1, released.size() - startAt, out);   
        }
    }

    if (out)
    {
        fclose(out);
    }

    
    if (!gotstatus || !checker.verify())
    {
        remove(outTdbgPath);
        outError = "checksum mismatch, response corrupted";
        return false;
    }
    return status == 0; 
   

    // Reply on the wire: [status][payload][CRC-32]. The CRC covers the status byte too.
    // Feed every received byte (status included) to a TrailerChecker.
    // Status 1: collect the payload into outError. Status 0: write the payload to the file.
    // At the end call verify(). If it fails, delete the file, set outError to
    // "checksum mismatch, response corrupted" and return false.
}

// =============================================================================================
// PART B: READING THE .tdbg FILE
// File layout (written by Phase 1):
//   header | snapshot 0 | snapshot 1 | ... | index (one 8 byte offset per snapshot)
// The index lets us jump straight to any step without reading the steps before it.
// =============================================================================================

bool openTdbg(const char* path, TdbgReader& r)
{
   ifstream in(path, ios::binary);
    if (!in.is_open()) return false;

    TTDBHeader H;
    in.read(H.magic, 4);
    in.read((char*)&H.version, 4);
    in.read((char*)&H.stepCount, 4);
    in.read((char*)&H.indexOffset, 8);
    if (!in) return false;                   

    if (H.magic[0] != 'T' || H.magic[1] != 'T' || H.magic[2] != 'D' || H.magic[3] != 'B')
        return false;

    in.seekg(0, ios::end);
    int64_t fileSize = in.tellg();
    if (H.stepCount <= 0 || H.indexOffset < 20 ||H.indexOffset + (int64_t)H.stepCount * 8 > fileSize)
        return false;

    in.seekg(H.indexOffset, ios::beg);
    vector<int64_t> idx(H.stepCount);
    in.read((char*)idx.data(), (streamsize)H.stepCount * 8);  
    if (!in) return false;
    for (int32_t i = 0; i < H.stepCount; i++)
        if (idx[i] < 20 || idx[i] >= H.indexOffset)
            return false;

    r.header = H;
    r.index = idx;
    r.file_index = 0;                     
    return true;
}
void closeTdbg(TdbgReader& r)
{

    r.index.clear();
    r.index.shrink_to_fit();
    r.file_index=-1;


    // Free r.index and close the file.
}
Snapshot* readSnapshotAt(TdbgReader& r, int32_t step)
{
    if(r.file_index<0 || step<0|| step >=r.header.stepCount)
    {
        return nullptr;
    }
    ifstream in(TDBG_FILE,ios::binary);

    if(!in.is_open())
    {
        return nullptr;
    }
    in.seekg(r.index[step],ios::beg);

    Snapshot* si=new Snapshot;
    int32_t len;
char buf[256];

   in.read((char*)&si->stackDepth, 4);
if (!in || si->stackDepth < 0 || si->stackDepth > MAX_STACK_DEPTH)
{
        delete si;
        return nullptr;
    }
    for(int i=0;i<si->stackDepth;i++)
    {
      Frame& f = si->callStack[i];
        in.read((char*)&len, 4);
        if (!in || len < 0 || len > 256)
         {
             delete si; 
             return nullptr;
         }
        in.read(buf, len);
        f.func_name.assign(buf, len);

        in.read((char*)&f.argc, 4);
        if (!in || f.argc < 0 || f.argc > MAX_VARS_PER_FRAME)
        {
            delete si;
            return nullptr;
        }
        for (int k = 0; k < f.argc; k++)
        {
          in.read((char*)&len, 4);
    if (!in || len < 0 || len > 256) { delete si; return nullptr; }
    in.read(buf, len);
    f.argv[k].name.assign(buf, len);
    in.read((char*)&f.argv[k].value, 4);
        }

        in.read((char*)&f.returnLine, 4);

        in.read((char*)&f.localCount, 4);
        if (!in || f.localCount < 0 || f.localCount > MAX_VARS_PER_FRAME)
        {
            delete si;
            return nullptr;
        }
        for (int k = 0; k < f.localCount; k++)
        {
           in.read((char*)&len, 4);
    if (!in || len < 0 || len > 256) 
    {
         delete si;
          return nullptr;
         }
    in.read(buf, len);
    f.locals[k].name.assign(buf, len);
    in.read((char*)&f.locals[k].value, 4);
        }
    }
    if(!in)
    {
        delete si;
        return nullptr;
    }
     return si;
}

// =============================================================================================
// PART C: THE SESSION IN MEMORY
// There is one DebugSession variable, created in main() and passed to every command by reference.
// =============================================================================================

void initSession(DebugSession& s)
{
    s.active_flag=0;
    s.reader.file_index=-1;
    s.currentStep=0;

}
void closeSession(DebugSession& s)
{
    if(s.active_flag)
    closeTdbg(s.reader);
    s.active_flag=0;
    // If active, call closeTdbg(s.reader). Then set active = false.
    // Called when the user types quit and when the input ends.
}

// =============================================================================================
// PART D: THE COMMANDS
// =============================================================================================

void printSnapshot(int32_t step, int32_t totalSteps, const Snapshot& s); // defined in Part E

bool showCurrent(DebugSession& s)
{
    Snapshot* snaps=readSnapshotAt(s.reader,(int32_t)s.currentStep);
    if(snaps==nullptr)
    {
        cout<<"damaged  .tdbg file"<<endl;
        return false;
    }

    printSnapshot((int32_t)s.currentStep+1,s.reader.header.stepCount,*snaps);
    delete snaps;
    return true;
    // Read the snapshot at s.currentStep, call printSnapshot, delete the snapshot.
    // If readSnapshotAt returns nullptr, print "damaged .tdbg file" and return false.
}
bool moveBy(DebugSession& s, int32_t delta)
{
    if(!s.active_flag)
    {
        cout<<"no session is used";
        return false;
    }
    int64_t tar=s.currentStep+(int64_t)delta;
    if(tar<0)
    {
        cout<<"target is already athe first step "<<endl;
        return false;
    }
      if(tar>=s.reader.header.stepCount)
      {
         cout << "already at the last step" << endl;
        return false;
      }
   int64_t oldsteps=s.currentStep;
   s.currentStep=tar;
   if(!showCurrent(s))
   {
    s.currentStep = oldsteps; 
        return false;
   }

   return true;
}
bool requestDebugFile(const char* host, int port, const char* sourcePath,
                      const char* outTdbgPath, string& outError)
{
    int sock = connectToHost(host, port);
    if (sock < 0)
    {
        outError = "cannot connect to load balancer";
        return false;
    }
    if (!uploadSource(sock, sourcePath))
    {
        outError = "cannot upload source file";
        close(sock);
        return false;
    }
    bool ok = receiveResponse(sock, outTdbgPath, outError);
    close(sock);
    return ok;
}
bool cmdStart(DebugSession& s, const string& sourcePath)
{
    // 1. requestDebugFile with host and port, saving to TDBG_FILE.
    // 2. If it fails, print the error text and return false. Keep the old session as it was.
    // 3. Open the new file with openTdbg into a NEW local TdbgReader.
    //    If that fails, print "server sent a damaged .tdbg file", keep the old session, return false.
    // 4. Only now close the old reader (if s.active), then put the new reader into s,
    //    set s.currentStep = 0 and s.active = true.
    // 5. Show step 1 with showCurrent(s).
    const char* NEWFILE="ttdb_new.tdbg";
    string error;
    if(!requestDebugFile(LB_HOST.c_str(),LB_PORT,sourcePath.c_str(),NEWFILE,error))
    {
        cout<<error<<endl;
        remove(NEWFILE);
        return false;
    }
    TdbgReader nreader;
    nreader.file_index=-1;
    if(!openTdbg(NEWFILE,nreader))
    {
        cout<<"server sent a damaged .tdbg file"<<endl;
        remove(NEWFILE);
        return false;
    }
    if(s.active_flag)
    closeTdbg(s.reader);

    remove(TDBG_FILE);
    if(rename(NEWFILE,TDBG_FILE)!=0)
    {
        cout << "cannot save the downloaded file" << endl;
        s.active_flag = 0;
        return false;
    }
    s.reader=nreader;
    s.currentStep=0;
    s.active_flag=1;
      return showCurrent(s);
}
bool cmdNext(DebugSession& s) {
    return moveBy(s,1);
}
bool cmdPrv(DebugSession& s) {
     return moveBy(s,-1);
}
bool cmdJmpNext(DebugSession& s, int32_t n) {
     return moveBy(s,n);
}
bool cmdJmpPrv(DebugSession& s, int32_t n) {
     return moveBy(s,-n);
}

// =============================================================================================
// PART E: PRINTING
// =============================================================================================

void printSnapshot(int32_t step, int32_t totalSteps, const Snapshot& s)
{
    cout<<"-------"<<step<<"of"<<totalSteps<<"-----"<<endl;
    cout<<"stack depth"<<s.stackDepth<<endl;

    for(int i=0;i<s.stackDepth;i++)
    {
         const Frame& fr = s.callStack[i];
         cout << "  [" << i << "] " << fr.func_name;
        if (i == 0)
            cout << "  (running now)";
        cout << endl;
        cout<<"  args"<<endl;
        if(fr.argc==0)
        {
            cout<<"none";
        }
        for(int i=0;i<fr.argc;i++)
        {
            cout<<fr.argv[i].name<<"="<<fr.argv[i].value<<" ";
        }
        cout<<endl;

        if (fr.localCount == 0)
        {
            cout << "none";

        }
        for (int k = 0; k < fr.localCount; k++)
        {
            cout << fr.locals[k].name << "=" << fr.locals[k].value << " ";

        }
        cout << endl;
    }
}
void printHelp()
{
    // Print the commands

    cout << "commands=" << endl;
    cout << "  start <file>     run a source file and show step 1" << endl;
    cout << "  next             go one step forward" << endl;
    cout << "  prv              go one step back" << endl;
    cout << "  jmp next <n>     go n steps forward" << endl;
    cout << "  jmp prv <n>      go n steps back" << endl;
    cout << "  help             show this text" << endl;
    cout << "  quit / exit      leave" << endl;
}

// =============================================================================================
// PART F: THE INFINITE LOOP
// =============================================================================================


bool readPositive(const string& text, int32_t& out)
{
    if(text.empty()||text.size()>10)
    {
        return false;
    }
    long long values=0;
    for(int i=0;i<(int)text.size();i++)
    {
        if(text[i]<'0'|| text[i]>'9')
        {
            return false;
        }
        values=values*10+(text[i]-'0');
    }
    if(values<=0 || values>MAX_JUMP)
    {
        return false;
    }
    out=(int32_t)values;
    return true;

}
bool runCommand(DebugSession& s, const string& line, bool& quitRequested)
{
    // 1. Split the line into words.
    // 2. If the first word is "ttdb", drop it.
    // 3. Match the first word:
    //      start      -> needs a path after it, then cmdStart
    //      next       -> no more words, then cmdNext
    //      prv        -> no more words, then cmdPrv
    //      jmp        -> second word "next" or "prv", third word a positive number ,
    //                    then cmdJmpNext or cmdJmpPrv
    //      help       -> printHelp
    //      quit, exit -> set quitRequested = true
    // 4. For anything else, or a wrong number of words, print "unknown command" and the help text, and return false.
    string words[10];
    int count=0;
    string current="";
    for(int i=0;i<(int)line.size();i++)
    
    {
        if(line[i]==' '|| line[i]=='\t')
        {
            if(current!=""&& count<10)
            {
                words[count++]=current;
                current="";
            }
        }
            else{
                current+=line[i];
            }
        
      
    }
       if (current != "" && count < 10)
        words[count++] = current;
        int srt=0;
        if(count>0 && words[0]==PROGRAM_NAME)
        {
            srt=1;

        }
        int n=count-srt;
        if(n>0)
        {
            string cmd = words[srt];

        if (cmd == "start" && n == 2)
        {
            return cmdStart(s, words[srt + 1]);
        }

        if (cmd == "next" && n == 1)
        {
            return cmdNext(s);
        }

        if (cmd == "prv" && n == 1)
        {
            return cmdPrv(s);
        }
            

            
        if (cmd == "jmp" && n == 3)
        {
             int32_t value;
           if (readPositive(words[srt + 2], value))
             {
                  if (words[srt + 1] == "next")
                  { return cmdJmpNext(s, value);

                  }
                if (words[srt + 1] == "prv") 
                
                {return cmdJmpPrv(s, value);
                }
            }
        }
             if(cmd=="help" && n==1)
        {
            printHelp();
             return true;
        }
        if((cmd=="quit"||cmd=="exit") && n==1)
        {
            quitRequested=true;
             return true;
        }

        }
        cout<<"unknown commands"<<endl;
        printHelp();
        return false;
}
int main(int argc, char* argv[])
{
    // you get string LB_HOST and int LB_PORT = 0 in arguments of this main function. 

    // DebugSession session; initSession(session);
    // Print a short welcome and the help text.
    //
    // Infinite loop:
    //   if quitRequested, break
    //
    // After the loop: closeSession(session) and return 0.
    // Never exit the loop because of an error. Only quit or the end of the input leaves it.
    if (argc >= 3)
    {
        LB_HOST = argv[1];
        LB_PORT = atoi(argv[2]);
    }

    DebugSession session;
    initSession(session);

    cout << "Time-Travel Debugger" << endl;
    printHelp();

    bool quitRequested = false;
    string line;
    while (!quitRequested)
    {
        cout << PROMPT;
        if (!getline(cin, line))
        {
            break;
        }
        if (line.find_first_not_of(" \t") == string::npos)
        {
            continue;
        }
        runCommand(session, line, quitRequested);
    }

    closeSession(session);
    return 0;
   
}