#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include "gr1context.hpp"
#undef fail

/**
 * @brief Constructor that reads the problem instance from file and prepares the BFManager, the BFVarCubes, and the BFVarVectors
 * @param inFile the input filename
 */
GR1Context::GR1Context(const char *inFileName) {

    std::ifstream inFile(inFileName);
    if (inFile.fail()) throw "Error: Cannot open input file";

    // Prepare safety and initialization constraints
    initEnv = mgr.constantTrue();
    initSys = mgr.constantTrue();
    safetyEnv = mgr.constantTrue();
    safetySys = mgr.constantTrue();

    // The readmode variable stores in which chapter of the input file we are
    int readMode = -1;
    std::string currentLine;
    lineNumberCurrentlyRead = 0;
    while (std::getline(inFile,currentLine)) {
        lineNumberCurrentlyRead++;
        boost::trim(currentLine);
        if ((currentLine.length()>0) && (currentLine[0]!='#')) {
            if (currentLine[0]=='[') {
                if (currentLine=="[INPUT]") {
                    readMode = 0;
                } else if (currentLine=="[OUTPUT]") {
                    readMode = 1;
                } else if (currentLine=="[ENV_INIT]") {
                    readMode = 2;
                } else if (currentLine=="[SYS_INIT]") {
                    readMode = 3;
                } else if (currentLine=="[ENV_TRANS]") {
                    readMode = 4;
                } else if (currentLine=="[SYS_TRANS]") {
                    readMode = 5;
                } else if (currentLine=="[ENV_LIVENESS]") {
                    readMode = 6;
                } else if (currentLine=="[SYS_LIVENESS]") {
                    readMode = 7;
                } else {
                    std::cerr << "Sorry. Didn't recognize category " << currentLine << "\n";
                    throw "Aborted.";
                }
            } else {
                if (readMode==0) {
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine);
                    variableTypes.push_back(PreInput);
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine+"'");
                    variableTypes.push_back(PostInput);
                } else if (readMode==1) {
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine);
                    variableTypes.push_back(PreOutput);
                    variables.push_back(mgr.newVariable());
                    variableNames.push_back(currentLine+"'");
                    variableTypes.push_back(PostOutput);
                } else if (readMode==2) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    initEnv &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==3) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreOutput);
                    initSys &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==4) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutput);
                    allowedTypes.insert(PostInput);
                    safetyEnv &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==5) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutput);
                    allowedTypes.insert(PostInput);
                    allowedTypes.insert(PostOutput);
                    safetySys &= parseBooleanFormula(currentLine,allowedTypes);
                } else if (readMode==6) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutput);
                    allowedTypes.insert(PostInput);
                    livenessAssumptions.push_back(parseBooleanFormula(currentLine,allowedTypes));
                } else if (readMode==7) {
                    std::set<VariableType> allowedTypes;
                    allowedTypes.insert(PreInput);
                    allowedTypes.insert(PreOutput);
                    allowedTypes.insert(PostInput);
                    allowedTypes.insert(PostOutput);
                    livenessGuarantees.push_back(parseBooleanFormula(currentLine,allowedTypes));
                } else {
                    std::cerr << "Error with line " << lineNumberCurrentlyRead << "!";
                    throw "Found a line in the specification file that has no proper categorial context.";
                }
            }
        }
    }

    // Compute VarVectors and VarCubes
    std::vector<BF> postOutputVars;
    std::vector<BF> postInputVars;
    std::vector<BF> preOutputVars;
    std::vector<BF> preInputVars;
    for (uint i=0;i<variables.size();i++) {
        switch (variableTypes[i]) {
        case PreInput:
            preVars.push_back(variables[i]);
            preInputVars.push_back(variables[i]);
            break;
        case PreOutput:
            preVars.push_back(variables[i]);
            preOutputVars.push_back(variables[i]);
            break;
        case PostInput:
            postVars.push_back(variables[i]);
            postInputVars.push_back(variables[i]);
            break;
        case PostOutput:
            postVars.push_back(variables[i]);
            postOutputVars.push_back(variables[i]);
            break;
        default:
            throw "Error: Found a variable of unknown type";
        }
    }
    varVectorPre = mgr.computeVarVector(preVars);
    varVectorPost = mgr.computeVarVector(postVars);
    varCubePostInput = mgr.computeCube(postInputVars);
    varCubePostOutput = mgr.computeCube(postOutputVars);
    varCubePreInput = mgr.computeCube(preInputVars);
    varCubePreOutput = mgr.computeCube(preOutputVars);
    varCubePre = mgr.computeCube(preVars);

    // Make sure that there is at least one liveness assumption and one liveness guarantee
    // The synthesis algorithm might be unsound otherwise
    if (livenessAssumptions.size()==0) livenessAssumptions.push_back(mgr.constantTrue());
    if (livenessGuarantees.size()==0) livenessGuarantees.push_back(mgr.constantTrue());
}

/**
 * @brief Recurse internal function to parse a Boolean formula from a line in the input file
 * @param is the input stream from which the tokens in the line are read
 * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
 * @return a BF that represents the transition constraint read from the line
 */
BF GR1Context::parseBooleanFormulaRecurse(std::istringstream &is,std::set<VariableType> &allowedTypes) {
    std::string operation = "";
    is >> operation;
    if (operation=="") {
        std::cerr << "Error reading line " << lineNumberCurrentlyRead << " from the input file. Premature end of line.\n";
        throw "Fatal Error";
    }
    if (operation=="|") return parseBooleanFormulaRecurse(is,allowedTypes) | parseBooleanFormulaRecurse(is,allowedTypes);
    if (operation=="&") return parseBooleanFormulaRecurse(is,allowedTypes) & parseBooleanFormulaRecurse(is,allowedTypes);
    if (operation=="!") return !parseBooleanFormulaRecurse(is,allowedTypes);
    if (operation=="1") return mgr.constantTrue();
    if (operation=="0") return mgr.constantFalse();

    // Has to be a variable!
    for (uint i=0;i<variableNames.size();i++) {
        if (variableNames[i]==operation) {
            if (allowedTypes.count(variableTypes[i])==0) {
                std::cerr << "Error reading line " << lineNumberCurrentlyRead << " from the input file. The variable " << operation << " is not allowed for this type of expression.\n";
                throw "Fatal Error";
            }
            return variables[i];
        }
    }
    std::cerr << "Error reading line " << lineNumberCurrentlyRead << " from the input file. The variable " << operation << " has not been found.\n";
    throw "Fatal Error";
}

/**
 * @brief Internal function for parsing a Boolean formula from a line in the input file - calls the recursive function to do all the work.
 * @param currentLine the line to parse
 * @param allowedTypes a list of allowed variable types - this allows to check that assumptions do not refer to 'next' output values.
 * @return a BF that represents the transition constraint read from the line
 */
BF GR1Context::parseBooleanFormula(std::string currentLine,std::set<VariableType> &allowedTypes) {

    std::istringstream is(currentLine);

    BF result = parseBooleanFormulaRecurse(is,allowedTypes);
    std::string nextPart = "";
    is >> nextPart;
    if (nextPart=="") return result;

    std::cerr << "Error reading line " << lineNumberCurrentlyRead << " from the input file. There are stray characters: '" << nextPart << "'" << std::endl;
    throw "Fatal Error";
}

/**
 * @brief The main function. Parses arguments from the command line and instantiates a synthesizer object accordingly.
 * @return the error code: >0 means that some error has occured. In case of realizability or unrealizability, a value of 0 is returned.
 */
int main(int argc, const char **args) {
    std::cerr << "SLUGS: SmaLl bUt complete Gr(1) Synthesis tool (see the documentation for an author list).\n";

    std::string filename = "";
    bool onlyCheckRealizability = false;

    for (int i=1;i<argc;i++) {
        std::string arg = args[i];
        if (arg[0]=='-') {
            if (arg=="--onlyRealizability") {
                onlyCheckRealizability = true;
            } else {
                std::cerr << "Error: Did not understand parameter " << arg << std::endl;
                return 1;
            }
        } else {
            if (filename=="") {
                filename = arg;
            } else {
                std::cerr << "Error: More than one input filename given.\n";
                return 1;
            }
        }
    }

    try {
        GR1Context context(filename.c_str());
        bool realizable = context.checkRealizability();
        if (realizable) {
            std::cerr << "RESULT: Specification is realizable.\n";
            if (!onlyCheckRealizability) context.computeAndPrintExplicitStateStrategy();
        } else {
            std::cerr << "RESULT: Specification is not realizable.\n";
        }


    } catch (const char *error) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }

    return 0;
}
