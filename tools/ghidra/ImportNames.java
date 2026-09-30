// Names the functions and globals of DAOrigins.exe from tab-separated
// `rva, name, kind` lines (tools/names.py --tsv): every address named in
// src/dao/Addresses.h, every message handler and script command's
// implementation from dev/, and every vftable from the exe's RTTI.
// `Class::Method` goes into the namespace `Class`; tools/names.py has
// already written `::` inside template brackets as `.`, so every `::` here
// is a scope.
// A function is created at a `function` row's address when analysis made
// none there (the script command registrar, for one, sits in code Ghidra
// found no way into); a `data` row, or an address where no function can be
// made, gets its name as the primary label. A name set by hand in Ghidra is
// left alone: a finding made there outranks one read from a header.
// Idempotent, so tools/ghidra-mcp.ps1 runs it on every launch and a name
// added to either source appears there.
//@category DAOLib

import java.nio.file.Files;
import java.nio.file.Path;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.NamespaceUtils;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolUtilities;

public class ImportNames extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length != 1) {
			printerr("usage: ImportNames.java <tsv of rva, name, kind>");
			return;
		}
		Address base = currentProgram.getImageBase();
		int functions = 0, labels = 0, kept = 0, failed = 0, created = 0;
		for (String line : Files.readAllLines(Path.of(args[0]))) {
			String[] fields = line.split("	", -1);
			if (fields.length < 2) {
				continue;
			}
			Address at = base.add(Long.parseLong(fields[0].trim(), 16));
			String full = fields[1].trim();
			boolean wantFunction = fields.length < 3 || !fields[2].trim().equals("data");
			int split = full.lastIndexOf("::");
			String simple = SymbolUtilities.replaceInvalidChars(
				split < 0 ? full : full.substring(split + 2), true);
			try {
				Namespace ns = split < 0 ? currentProgram.getGlobalNamespace()
						: NamespaceUtils.createNamespaceHierarchy(full.substring(0, split), null,
							currentProgram, SourceType.IMPORTED);
				Function function = getFunctionAt(at);
				if (function == null && wantFunction && currentProgram.getMemory().getBlock(at) != null
						&& currentProgram.getMemory().getBlock(at).isExecute()) {
					// Disassembles from here if nothing is, then bounds the function.
					// Created with the default name: a name passed here would be
					// recorded as the user's, and never namespaced or updated again.
					function = createFunction(at, null);
					if (function != null) {
						created++;
					}
				}
				Symbol symbol = function != null ? function.getSymbol()
						: currentProgram.getSymbolTable().getPrimarySymbol(at);
				// A user-defined symbol that already carries our name is one an
				// earlier version of this script created that way; it is ours.
				if (symbol != null && symbol.getSource() == SourceType.USER_DEFINED
						&& !symbol.getName().equals(simple)) {
					kept++;
				}
				else if (function != null && symbol.getName().equals(simple)) {
					// Already named. Ghidra's class recovery may have moved it into
					// its class's namespace (a class, where ours is a plain
					// namespace of the same name); that placement is the better one.
					functions++;
				}
				else if (function != null) {
					symbol.setNameAndNamespace(simple, ns, SourceType.IMPORTED);
					functions++;
				}
				else {
					currentProgram.getSymbolTable()
							.createLabel(at, simple, ns, SourceType.IMPORTED)
							.setPrimary();
					labels++;
				}
			}
			catch (Exception e) {
				// The first few say what went wrong; a thousand would bury the summary.
				if (failed < 20) {
					printerr(full + " at " + at + ": " + e.getMessage());
				}
				failed++;
			}
		}
		println(String.format(
			"ImportNames: %d functions (%d created), %d labels, %d named by hand kept, %d failed",
			functions, created, labels, kept, failed));
	}
}
