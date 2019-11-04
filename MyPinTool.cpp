/*
 * Copyright 2002-2019 Intel Corporation.
 *
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 *
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <set>
#include "pin.H"

using std::cerr;
using std::ofstream;
using std::ios;
using std::string;
using std::map;
using std::set;
using std::pair;
using std::endl;

FILE* out_file = NULL;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
static UINT64 icount = 0;
static ADDRINT prev_addr = 0;
static map<ADDRINT, set<ADDRINT>> addr_map;
static int threadcount = 0;

// This function is called before every instruction is executed
// It tracks the IP flow
VOID track_ip(VOID* vip)
{
	icount++;
	if (icount < addr_map.max_size())
	{
		ADDRINT ip = (ADDRINT)vip;
		if (addr_map.find(prev_addr) == addr_map.end())
		{
			addr_map[prev_addr] = set<ADDRINT>();
		}
		addr_map[prev_addr].insert(ip);
		prev_addr = ip;
	}
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID* v)
{
	// Insert a call to docount before every instruction, no arguments are passed
	IMG img = IMG_FindByAddress(INS_Address(ins));
	if (IMG_Valid(img) && IMG_IsMainExecutable(img))
	{
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)track_ip, IARG_INST_PTR, IARG_END);
	}
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
	"o", "iptrack.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID* v)
{
	LOG("Fini\n");
	if (out_file != NULL)
	{
		// Write to a file since cout and cerr maybe closed by the application
		addr_map.erase(addr_map.find(0));
		fprintf(out_file, "digraph controlflow {\n");
		for (map<ADDRINT, set<ADDRINT>>::iterator it_map = addr_map.begin(); it_map != addr_map.end(); ++it_map)
		{
			for (set<ADDRINT>::iterator it_set = it_map->second.begin(); it_set != it_map->second.end(); ++it_set)
			{
				fprintf(out_file, "\t\"%p\" -> \"%p\";\n", (void*)it_map->first, (void*)*it_set);
			}
		}
		fprintf(out_file, "}\n");
		fclose(out_file);
		out_file = NULL;
	}
}

VOID ThreadStart(THREADID threadIndex, CONTEXT* ctxt, INT32 flags, VOID* v)
{
	threadcount++;
	std::stringstream ss;
	ss << "ThreadStart id:" << threadIndex << " -- " << PIN_ThreadId() << " -- " << threadcount << endl;
	string str = ss.str();
	LOG(str);
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT* ctxt, INT32 code, VOID* v)
{
	threadcount--;
	std::stringstream ss;
	ss << "ThreadFini id:" << threadIndex << " -- " << PIN_ThreadId() << " -- " << threadcount << endl;
	string str = ss.str();
	LOG(str);
	if (threadcount == 0)
	{
		Fini(code, v);
	}
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
	cerr << "This tool counts the number of dynamic instructions executed" << endl;
	cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
	return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

int main(int argc, char* argv[])
{
	// Initialize pin
	if (PIN_Init(argc, argv)) return Usage();

	//OutFile.open(KnobOutputFile.Value().c_str());
	out_file = fopen("iptrack.out", "w");

	// Register Instruction to be called to instrument instructions
	INS_AddInstrumentFunction(Instruction, 0);

	// Follow Thread starts
	PIN_AddThreadStartFunction(ThreadStart, 0);

	// Handle abrubt closings
	PIN_AddThreadFiniFunction(ThreadFini, 0);

	// Register Fini to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}
