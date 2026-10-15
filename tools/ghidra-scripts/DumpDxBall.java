//Dump Ghidra analysis of DXBALL.EXE into notes/.
//@category DXBall
//@runtime Java

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Program;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SymbolType;

public class DumpDxBall extends GhidraScript {

	@Override
	public void run() throws Exception {
		Program program = currentProgram;
		File outDir = new File("/home/scp1/dev/dxball/notes");
		outDir.mkdirs();
		File decompDir = new File(outDir, "decomp");
		decompDir.mkdirs();

		dumpFunctions(program, new File(outDir, "functions.txt"));
		dumpImports(program, new File(outDir, "imports.txt"));
		dumpStrings(program, new File(outDir, "ghidra-strings.txt"));
		dumpBlocks(program, new File(outDir, "memory-blocks.txt"));
		decompileAll(program, decompDir, new File(outDir, "decomp.c"));
	}

	private void dumpFunctions(Program program, File file) throws Exception {
		PrintWriter out = new PrintWriter(file, "UTF-8");
		FunctionIterator it = program.getFunctionManager().getFunctions(true);
		int n = 0;
		while (it.hasNext()) {
			Function fn = it.next();
			long size = fn.getBody().getNumAddresses();
			out.printf("%s\t%s\t%s\tsize=%d\tparams=%d\n",
					fn.getEntryPoint(),
					fn.getName(),
					fn.getCallingConventionName(),
					size,
					fn.getParameterCount());
			n++;
		}
		out.printf("# functions=%d\n", n);
		out.close();
		println("Wrote " + n + " functions");
	}

	private void dumpImports(Program program, File file) throws Exception {
		PrintWriter out = new PrintWriter(file, "UTF-8");
		SymbolTable table = program.getSymbolTable();
		SymbolIterator it = table.getExternalSymbols();
		List<String> rows = new ArrayList<String>();
		while (it.hasNext()) {
			Symbol sym = it.next();
			rows.add(sym.getParentNamespace().getName() + "\t" + sym.getName() + "\t" +
					sym.getAddress());
		}
		Collections.sort(rows);
		for (String row : rows) {
			out.println(row);
		}
		out.close();
		println("Wrote " + rows.size() + " imports");
	}

	private void dumpStrings(Program program, File file) throws Exception {
		PrintWriter out = new PrintWriter(file, "UTF-8");
		SymbolIterator it = program.getSymbolTable().getDefinedSymbols();
		int n = 0;
		while (it.hasNext()) {
			Symbol sym = it.next();
			if (sym.getSymbolType() != SymbolType.LABEL) {
				continue;
			}
			String name = sym.getName();
			if (!name.startsWith("s_") && !name.startsWith("u_") && !name.startsWith("PTR_s_")) {
				continue;
			}
			out.printf("%s\t%s\n", sym.getAddress(), name);
			n++;
		}
		out.close();
		println("Wrote " + n + " string labels");
	}

	private void dumpBlocks(Program program, File file) throws Exception {
		PrintWriter out = new PrintWriter(file, "UTF-8");
		for (MemoryBlock block : program.getMemory().getBlocks()) {
			String perm = (block.isRead() ? "r" : "-") +
					(block.isWrite() ? "w" : "-") +
					(block.isExecute() ? "x" : "-");
			out.printf("%s\t%s\t%s\tsize=%d\t%s\n",
					block.getName(),
					block.getStart(),
					block.getEnd(),
					block.getSize(),
					perm);
		}
		out.close();
	}

	private void decompileAll(Program program, File decompDir, File combined) throws Exception {
		DecompInterface decomp = new DecompInterface();
		decomp.openProgram(program);
		PrintWriter all = new PrintWriter(combined, "UTF-8");
		FunctionIterator it = program.getFunctionManager().getFunctions(true);
		int ok = 0;
		int fail = 0;
		while (it.hasNext() && !monitor.isCancelled()) {
			Function fn = it.next();
			String name = fn.getName().replaceAll("[^A-Za-z0-9_@]", "_");
			String addr = fn.getEntryPoint().toString();
			DecompileResults result = decomp.decompileFunction(fn, 60, monitor);
			String c;
			if (result != null && result.decompileCompleted()) {
				c = result.getDecompiledFunction().getC();
				ok++;
			} else {
				c = "/* decompile failed: " + (result == null ? "null" : result.getErrorMessage()) + " */\n";
				fail++;
			}
			String header = "/* " + fn.getName() + " @ " + addr + " size=" +
					fn.getBody().getNumAddresses() + " */\n";
			all.print(header);
			all.print(c);
			all.print("\n\n");
			PrintWriter one = new PrintWriter(new File(decompDir, addr + "_" + name + ".c"), "UTF-8");
			one.print(header);
			one.print(c);
			one.close();
		}
		all.close();
		decomp.dispose();
		println("Decompiled ok=" + ok + " fail=" + fail);
	}
}
