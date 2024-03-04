#include <cassert>

#include "LocalParameters.h"
#include "FileUtil.h"
#include "CommandCaller.h"
#include "Util.h"
#include "Debug.h"

#include "complexsearch.sh.h"


int complexsearch(int argc, const char **argv, const Command &command) {
    LocalParameters &par = LocalParameters::getLocalInstance();
    par.PARAM_ADD_BACKTRACE.addCategory(MMseqsParameter::COMMAND_EXPERT);
    par.PARAM_MAX_REJECTED.addCategory(MMseqsParameter::COMMAND_EXPERT);
    par.PARAM_ZDROP.addCategory(MMseqsParameter::COMMAND_EXPERT);
    par.PARAM_DB_OUTPUT.addCategory(MMseqsParameter::COMMAND_EXPERT);
    par.PARAM_OVERLAP.addCategory(MMseqsParameter::COMMAND_EXPERT);
    par.PARAM_RESCORE_MODE.addCategory(MMseqsParameter::COMMAND_EXPERT);
    for (size_t i = 0; i < par.createdb.size(); i++){
        par.createdb[i]->addCategory(MMseqsParameter::COMMAND_EXPERT);
    }

    par.PARAM_COMPRESSED.removeCategory(MMseqsParameter::COMMAND_EXPERT);
    par.PARAM_THREADS.removeCategory(MMseqsParameter::COMMAND_EXPERT);
    par.PARAM_V.removeCategory(MMseqsParameter::COMMAND_EXPERT);

    par.parseParameters(argc, argv, command, false, Parameters::PARSE_VARIADIC, 0);
    if(par.PARAM_FORMAT_OUTPUT.wasSet == false){
        par.outfmt = "query,target,fident,alnlen,mismatch,gapopen,qstart,qend,tstart,tend,evalue,bits,complexassignid";
    }
    par.addBacktrace = true;
    par.PARAM_ADD_BACKTRACE.wasSet = true;
    par.printParameters(command.cmd, argc, argv, *command.params);

    bool needBacktrace = false;
    bool needTaxonomy = false;
    bool needTaxonomyMapping = false;
    bool needLookup = false;
    {
        bool needSequenceDB = false;
        bool needFullHeaders = false;
        bool needSource = false;
        bool needQCA = false;
        bool needTCA = false;
        bool needTMalign = false;
        bool needLDDT = false;
        LocalParameters::getOutputFormat(par.formatAlignmentMode, par.outfmt, needSequenceDB, needBacktrace, needFullHeaders,
                                         needLookup, needSource, needTaxonomyMapping, needTaxonomy, needQCA, needTCA, needTMalign, needLDDT);
    }

    if (par.formatAlignmentMode == Parameters::FORMAT_ALIGNMENT_SAM ||
        par.formatAlignmentMode == LocalParameters::FORMAT_ALIGNMENT_PDB_SUPERPOSED  ||
        par.greedyBestHits) {
        needBacktrace = true;
    }
//    if (needBacktrace) {
//        Debug(Debug::INFO) << "Alignment backtraces will be computed, since they were requested by output format.\n";
//        par.addBacktrace = true;
//        par.PARAM_ADD_BACKTRACE.wasSet = true;
//    }
    if (needLookup) {
        par.writeLookup = true;
    }

    std::string tmpDir = par.filenames.back();
    std::string hash = SSTR(par.hashParameter(command.databases, par.filenames, *command.params));
    if (par.reuseLatest) {
        hash = FileUtil::getHashFromSymLink(tmpDir + "/latest");
    }
    tmpDir = FileUtil::createTemporaryDirectory(tmpDir, hash);
    par.filenames.pop_back();
    CommandCaller cmd;
    double eval =  par.evalThr;
    if(par.alignmentType == LocalParameters::ALIGNMENT_TYPE_TMALIGN){
        par.evalThr = par.eValueThrExpandComplex;
        cmd.addVariable("COMPLEX_ALIGNMENT_ALGO", "tmalign");
        cmd.addVariable("COMPLEX_ALIGN_PAR", par.createParameterString(par.tmalign).c_str());
    }else if(par.alignmentType == LocalParameters::ALIGNMENT_TYPE_3DI_AA || par.alignmentType == LocalParameters::ALIGNMENT_TYPE_3DI){
        par.evalThr = par.eValueThrExpandComplex;
        cmd.addVariable("COMPLEX_ALIGNMENT_ALGO", "structurealign");
        cmd.addVariable("COMPLEX_ALIGN_PAR", par.createParameterString(par.structurealign).c_str());
    }
    par.evalThr = eval;
    switch(par.prefMode){
        case LocalParameters::PREF_MODE_KMER:
            cmd.addVariable("PREFMODE", "KMER");
            break;
        case LocalParameters::PREF_MODE_UNGAPPED:
            cmd.addVariable("PREFMODE", "UNGAPPED");
            break;
        case LocalParameters::PREF_MODE_EXHAUSTIVE:
            cmd.addVariable("PREFMODE", "EXHAUSTIVE");
            break;
    }
    if(par.exhaustiveSearch){
        cmd.addVariable("PREFMODE", "EXHAUSTIVE");
    }
    cmd.addVariable("NO_REPORT", par.complexReportMode == 0 ? "TRUE" : NULL);
    cmd.addVariable("TMP_PATH", tmpDir.c_str());
    cmd.addVariable("OUTPUT", par.filenames.back().c_str());
    par.filenames.pop_back();
    cmd.addVariable("TARGETDB", par.filenames.back().c_str());
    par.filenames.pop_back();
    cmd.addVariable("QUERYDB", par.filenames.back().c_str());
    cmd.addVariable("LEAVE_INPUT", par.dbOut ? "TRUE" : NULL);
    par.filenames.pop_back();

    // initial search speed up!
    par.addBacktrace = par.exhaustiveSearch;
    par.alignmentType = par.exhaustiveSearch ? par.alignmentType : LocalParameters::ALIGNMENT_TYPE_3DI_AA;

    cmd.addVariable("SEARCH_PAR", par.createParameterString(par.structuresearchworkflow, true).c_str());
    cmd.addVariable("SCORECOMPLEX_PAR", par.createParameterString(par.scorecomplex).c_str());
    cmd.addVariable("THREADS_PAR", par.createParameterString(par.onlythreads).c_str());
    cmd.addVariable("REMOVE_TMP", par.removeTmpFiles ? "TRUE" : NULL);
    cmd.addVariable("VERBOSITY", par.createParameterString(par.onlyverbosity).c_str());
    std::string program = tmpDir + "/complexsearch.sh";
    FileUtil::writeFile(program, complexsearch_sh, complexsearch_sh_len);
    cmd.execProgram(program.c_str(), par.filenames);
    // Should never get here
    assert(false);
    return EXIT_FAILURE;
}
