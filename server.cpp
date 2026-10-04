// ======================= TIME-TRAVEL DEBUGGER - SERVER TEMPLATE =======================

// Pipeline this file implements, top to bottom:
//   0. Receive  -- stream the client's .trace bytes straight to source.bin on disk
//   1. Pass 0X0   -- validity check (FUNC/FUNC_END matching)
//   2. Pass 0X1   -- resolve(): copy EVERY source line into resolve.bin as [offset][size][string], then patch CALL targets.
//   3. Pass 0X2   -- execute resolve.bin: tokenize ONE line at a time, update the call stack, take a snapshot -> Timeline
//   4. Pass 0X3   -- serialize Timeline -> session.tdbg(header + snapshot records + dense index)

#include <iostream>
#include <string>
#include<cstdlib>
#include <cstdint>
#include <fstream>
#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#endif
#include <cstdio>
using namespace std;

// ---- Constants ----
const int32_t MAX_VARS_PER_FRAME = 16;
const int32_t MAX_STACK_DEPTH = 64;
const int32_t MAX_FUNCS = 128;
const int32_t MAX_TOKENS = MAX_VARS_PER_FRAME + 2; // kW + func_name + upto 16 params/args
const int32_t MAX_PATCHES = MAX_FUNCS * 4;
const uint64_t MAX_SOURCE_BYTES = 15ULL * 1024 * 1024; // sanity cap on the declared file length
const int32_t IO_BUFFER_SIZE = 64 * 1024;                  // fixed buffer for streaming to/from disk
const int32_t SOCKET_TIMEOUT_SEC = 5;                      // TODO: apply as SO_RCVTIMEO so a deadclient can't hang the server forever

// ---- Custom data structures

// Stack: back the live Call Stack during execution
template <typename T>
class Stack
{
    struct Node
    {
        T data;
        Node* next;
    };
    Node* top;
    int32_t count;

public:
    // Implement these functions:
    Stack()

    {

        top = nullptr;
        count = 0;// initialize the stack
    }
    void push(const T& val)
    {
        if (count >= MAX_STACK_DEPTH)
        {
            return;
        }
        Node* temp = new Node;
        temp->data = val;
        temp->next = top;
        top = temp;
        count++;


        // pushes the value on the stack if max limit is not reached yet.
    }
    T pop()
    {
        Node* n = top;
        T val = n->data;
        top = n->next;
        delete n;
        count--;
        return val;

        // pop the top value on the stack
    }
    T& peek()
    {
        return top->data;// returns the top value on the stack
    }
    bool isEmpty()
    {
        return top == nullptr;
    }
    int32_t depth()
    {
        return count;
    }
    int32_t snapshot_into(T out[], int32_t maxLen)
    {
        
        Node* cur = top;
        int32_t i = 0;
        while (cur != nullptr && i < maxLen)
        {
            out[i] = cur->data;
            i++;
            cur = cur->next;
        }
        return i;
            
            
            // copies every frame, top to bottom in the array given as a parameter
        // this is what buildSnapshot() call, returns count written
    }
};


// Timeline : doubly linked list of Snapshots
struct Snapshot; // fwd declaration;
struct TimelineNode
{
    Snapshot* data;
    TimelineNode* next;
    TimelineNode* prev;
};
class Timeline
{
    TimelineNode* head, * tail;
    int32_t stepCount;

public:
    // Implement these functions
    Timeline()
    {
        head = nullptr;
        tail = nullptr;
        stepCount = 0;
    }
    void record(Snapshot* s)
    {
        TimelineNode* n = new TimelineNode;
        n->data = s;
        n->next = nullptr;
        n->prev = tail;
        if (tail != nullptr)
            tail->next = n;
        else
            head = n;
        tail = n;
        stepCount++;
        // add record in the timeline
    }
    TimelineNode* begin()
    {
        return head;

    }
    int32_t getStepCount()
    {
        return stepCount;
    }
};

// Core structs
struct Variable
{
    string name;
    int32_t value;
};
struct Frame
{
    string func_name;
    int32_t argc;
    Variable argv[MAX_VARS_PER_FRAME];
    int32_t returnLine;
    Variable locals[MAX_VARS_PER_FRAME];
    int32_t localCount;
};
struct Snapshot
{
    Frame callStack[MAX_STACK_DEPTH];
    int32_t stackDepth;
};
struct TTDBHeader
{
    char magic[4]; // "TTDB"
    int32_t version;
    int32_t stepCount;
    int64_t indexOffset;
};
void writeHeader(FILE* f, const TTDBHeader& h)
{
    fwrite(h.magic, 1, 4, f);
    fwrite(&h.version, sizeof(int32_t), 1, f);

    // placeholder for other two data members
}

// resolve.bin - bookkeeping
struct FuncEntry
{
    string funcName;
    int64_t byteOffsetInResolveBin; // where this function's FUNC header record sits
};
struct PendingPatch
{
    int64_t byteOffsetOfOffsetField; // where in resolve.bin to seek back and overwrite
    string targetFuncName;
};



// PASS 0x0: READING source.bin + VALIDITY CHECK
bool readSourceLine(ifstream& in, string& out)
{
    
    string line;
    while (getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.find_first_not_of(" \t") == string::npos)
        {
            continue;
        }
        out = line;
        return true;
    }
    return false;
    
    // reads the next nonblank line
}
string firstWord(const string& line)
{
    string word = "";
    int i = 0;
    while (i < line.size() && (line[i] == ' '|| line[i]=='\t'))
    {
        i++;
    }
    while (i < line.size() && line[i] != ' '&& line[i]!='\t')
    {
        word += line[i];
        i++;
    }
    return word;
    // returns first word from the input string
}
string secondWord(const string& line)
{
    string word = "";
    int i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i]=='\t'))
    {
        i++;
    }
    while (i < line.size() && line[i] != ' ' && line[i] != '\t')
    {
        i++;
    }
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
    {
        i++;
    }
    while (i < line.size() && line[i] != ' ' && line[i] != '\t')
    {
        word += line[i];
        i++;
    }
    return word;
    // returns the second word
}
bool validateProgram(const char* sourcePath)
{
    ifstream in(sourcePath);
        if (!in.is_open())
        {
            cout << "ERROR:CANNOT OPEN!" << sourcePath<<endl;
            return false;
        }
    
    Stack<string> funcstack;
    string line;
    while (readSourceLine(in, line))
    {
        string keyword = firstWord(line);
        if (keyword == "func")
        {
            if (!funcstack.isEmpty())
            {
                cout << "nested function declaration!" << line << endl;
                return false;
            }
            funcstack.push(secondWord(line));
        }
            
            else if (keyword=="func_end")
            {
                if (funcstack.isEmpty())
                {
                    cout << "ERROR:func_end without matching func" << endl;
                    return false;
                }
                funcstack.pop();
            }
        
    }
    if (!funcstack.isEmpty())
    {
        cout << "ERROR:func without func_end" << endl;
        return false;

    }
    return true;

    // for each func defined there should be exactly one func_end and no nested funcs allowed - 
}

// PASS 0x1: RESOLVE() -> resolve.bin
int64_t writeResolveRecord(FILE* f, int64_t offsetField, const string& text)
{
    
    int64_t startpos = ftell(f);
    int32_t size = (int32_t)text.size();
    fwrite(&offsetField, sizeof(int64_t), 1, f);
    fwrite(&size, sizeof(int32_t), 1, f);
    fwrite(text.c_str(), 1, size, f);
    return startpos;
    
    // writes one [offset(8B)][size(4B)][string] record at the current file position
    // returns this record's own starting byte position
}
int64_t readResolveRecord(FILE* f, string& outText)
{

    int64_t offsetfield;
    int32_t size;
    if (fread(&offsetfield, sizeof(int64_t), 1, f) != 1)
        return -1;
    if (fread(&size, sizeof(int32_t), 1, f) != 1)
        return -1;
    char buffer[256];
    if (size < 0 || size >= 256)
    {
        return -1;
    }
    fread(buffer, 1, size, f);
    outText = string(buffer, size);
    return offsetfield;
    
    // reads one record at the current position and advances past it, returns the offset field - the raw line text comes back untouched in outText.
}
int64_t resolveProgram(const char* sourcePath, const char* resolveBinPath)
{
    FuncEntry funcArray[MAX_FUNCS];
    int32_t funcCount = 0;
    PendingPatch patches[MAX_PATCHES];
    int32_t patchCount = 0;
    ifstream in(sourcePath);
    FILE* f = fopen(resolveBinPath, "wb+");
    if (!in.is_open() || f == nullptr)

    {
        cout << "error files cannot open!"<<endl;
        return -1;
    }
    int64_t offset = 0;
    string line;
    while (readSourceLine(in, line))
    {
        int64_t pos = writeResolveRecord(f, offset, line);
        string keyword = firstWord(line);
        if (keyword == "func" && funcCount < MAX_FUNCS)
        {
            funcArray[funcCount].funcName = secondWord(line);
            funcArray[funcCount].byteOffsetInResolveBin = pos;
            funcCount++;
        }
        else if (keyword == "call" && patchCount < MAX_PATCHES)
        {
            patches[patchCount].byteOffsetOfOffsetField = pos;
            patches[patchCount].targetFuncName = secondWord(line);

            patchCount++;
        }
        offset += 8 + 4 + line.size();

    }
        for (int i = 0; i < patchCount; i++)
        {
            int64_t target = -1;
            for (int j = 0; j < funcCount; j++)
            {
                if (funcArray[j].funcName == patches[i].targetFuncName)
                {
                    target = funcArray[j].byteOffsetInResolveBin;
                    break;
                }
            }
            if (target == -1)
            {
                cout << "ERROR:call to undefined function"<<patches[i].targetFuncName<<endl;
                fclose(f);
                return -1;
            }
            fseek(f, patches[i].byteOffsetOfOffsetField, SEEK_SET);
            fwrite(&target, sizeof(int64_t), 1, f);
        }
        int64_t mainOffset = -1;
        for (int i = 0; i < funcCount; i++)
        {
            if (funcArray[i].funcName == "main")
            {
                mainOffset = funcArray[i].byteOffsetInResolveBin;
            }
      
        }
        fclose(f);
        if (mainOffset == -1)
        {
            cout << "error:no main function";
            return -1;
        }

    

    return mainOffset;
    // Every source line becomes one record holding the raw line, as-is.
    // resolve() only PEEKS at the leading word(s) -- enough to spot FUNC
    // (remember its position) and CALL (remember which function it needs
    // and where its offset field sits).
    // Once the whole file is written, every CALL's offset field is patched
    // Once the whole file is written, every CALL's offset field is patsched
    // with its target's position. Patching happens after the full write
    // Returns the byte offset of main's FUNC header record.
    // if there is no main return the error 
}

// PASS 0x2: EXECUTION (tokenization happens here)
enum TokenType
{
    KEYWORD,
    IDENTIFIER,
    PARAM
};
struct Token
{
    TokenType type;
    string text;
};
int32_t tokenizeLine(const string& line, Token tokens[], int32_t maxTokens)
{
    int32_t count = 0;
    int i = 0;
    while (i < (int)line.size() && count < maxTokens)
    {
        while (i < (int)line.size() && (line[i] == ' ' || line[i] == '\t'))
        {
            i++;
        }
        if (i >= (int)line.size())
        {
            break;
        }
        string word = "";
        while (i < (int)line.size() && line[i] != ' ' && line[i] != '\t')
        {
            word += line[i];
            i++;
        }
        if (count == 0)
        {
            tokens[count].type = KEYWORD;
        }
        else if (count == 1)
        {
            tokens[count].type = IDENTIFIER;
        }
        else
        {
            tokens[count].type = PARAM;
        }
        tokens[count].text = word;
        count++;


    }
    return count;
    
    // first word is always a instruction keyword
    // instruction set = [func, func_end, call, set, add, sub, mul and div]
    // next word is identifier like name of a function, variable name
    // after identifier all are the params/arg, space separated
}
Snapshot* buildSnapshot(Stack<Frame>& callStack)
{
    Snapshot* s = new Snapshot;
    s->stackDepth = callStack.snapshot_into(s->callStack, MAX_STACK_DEPTH);
    return s;                                                                                                                                                                                                                                                                                                                                                                                                            
    // build the snapshot based on the callStack given
}
bool isnumber(const string& s)
{
    if (s.size() == 0)
    {
        return false;
    }
    int start = 0;
    if (s[0] == '-')
    {
        start = 1;
    }
    if (start == s.size())
    {
        return false;
    }
    for (int i = start; i < s.size(); i++)
    {
        if (s[i] < '0' || s[i]>'9')
        {
            return false;
        }
    }
    return true;
}
int32_t* findvar(Frame& f, const string& name)
{
    for (int i = 0; i < f.argc; i++)
    {
        if (f.argv[i].name == name)
        {
            return &f.argv[i].value;
        }
    }
    for (int i = 0; i < f.localCount; i++)
    {
        if (f.locals[i].name == name)
        {
            return &f.locals[i].value;
        }
    }
    return nullptr;
}
int32_t* getcreate(Frame& f, const string& name)
{
    int32_t* p = findvar(f, name);
    if (p != nullptr)
    {
        return p;
    }
    if (f.localCount >= MAX_VARS_PER_FRAME)
    {
        return nullptr;
    }
    for (int i = 0; i < f.argc; i++)
    {
        if (f.argv[i].name == name)
        {
            return &f.argv[i].value;
        }
    }
    for (int i = 0; i < f.localCount; i++)
    {
        if (f.locals[i].name == name)
        {
            return &f.locals[i].value;
        }
    }
    return nullptr;
}
int32_t valueof(Frame& f, const string& s)
{
    if (isnumber(s))
    {
        return atoi(s.c_str());

    }
    int32_t* p = findvar(f, s);
    if (p == nullptr)
    {
        return 0;
    }
    return *p;
}
void executeProgram(const char* resolveBinPath, int64_t mainOffset, Timeline& timeline)
{
    // initialize the call stack
    // make the main frame
    // push main frame on the call stack

    // implementation:
    // execute line by line, and according to the keyword perform action
}

// PASS 0x3: SERIALIZE TIMELINE
void writeTdbg(Timeline& timeline, const char* tdbgPath)
{
    // placeholder for header
    // index array of the size of stepcount from the timeline
    // placing each snapshot in the file while maintaining the index(starting point of each nth snapshot)
    // after timeline add the index array i the file
    // update the header
}
// main section
int32_t main()
{

    if (!validateProgram("source.bin"))
    {
       
        // send an error response instead of a .tdbg file
        return 1;
     

    }
    cout << "VALID!" << endl;

    int64_t mainOffset = resolveProgram("source.bin", "resolve.bin");
    if (mainOffset < 0)
    {
        return 1;
    }
    cout << "main at" << mainOffset << endl;
    Token t[MAX_TOKENS];
    int n = tokenizeLine("add b a", t, MAX_TOKENS);
    for (int i = 0; i < n; i++)
    {
        cout << t[i].type << ":" << t[i].text << endl;
    }

    return 0;
}