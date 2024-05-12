//
// Created by LiShangyu on 2024/3/1.
//

#include "AbstractExecution/SVFIR2ItvExeState.h"
#include "Graphs/SVFG.h"
#include "SVF-LLVM/LLVMUtil.h"
#include "SVF-LLVM/SVFIRBuilder.h"
#include "SYN/LightAnalysis.h"
#include "Util/CommandLine.h"
#include "Util/Options.h"
#include "Util/Z3Expr.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace llvm;
using namespace SVF;

#define call_order 0
#define branch_order 1
#define define_order 2

static Option<std::string> SOURCEPATH("srcpath",
                                      "Path for source code to transform", "");

static Option<std::string> NEWSPECPATH("newspec",
                                       "Path for new specification file", "");

void traverseOnSVFStmt(const ICFGNode* node, SVF::Modification* modification)
{
    auto str = SOURCEPATH();
    // auto modification = new SVF::Modification(str);
    auto lightAnalysis = new LightAnalysis(str);
    // std::string stmtstring = node->toString();
    if (const CallICFGNode* callNode = SVFUtil::dyn_cast<CallICFGNode>(node))
    {
        const SVFInstruction* cs = callNode->getCallSite();
        std::string m = cs->getSourceLoc();
        std::string inststring = cs->toString();
        std ::cout << inststring << std::endl;
        if (m == "")
        {
            return;
        }
        std::string::size_type pos = m.find("\"ln\":");
        int num = std::stoi(m.substr(pos + 5, m.find(",") - pos - 5));
        std::regex re("@(\\w+)\\((.+)\\)");
        std::smatch match;
        std::string functionName;
        pos = m.find("\"fl\": \"");
        std::string srcpath = m.substr(pos + 7, m.find("\" }") - pos - 7);

        std::vector<std::string> parameters;
        if (std::regex_search(inststring, match, re) && match.size() > 2)
        {
            functionName = match.str(1);
            std::string parametersStr = match.str(2);
            std::stringstream ss(parametersStr);
            std::string parameter;
            while (std::getline(ss, parameter, ','))
            {
                parameter.erase(0, parameter.find_first_not_of(' '));
                parameter.erase(parameter.find_last_not_of(' ') + 1);
                parameters.push_back(parameter);
            }
        }
        if (functionName == "")
        {
            return;
        }
        std::cout << functionName << std::endl;
        std ::cout << srcpath << std::endl;
        lightAnalysis->findNodeOnTree(num, call_order, functionName, parameters,
                                      srcpath);
    }
    for (const SVFStmt* stmt : node->getSVFStmts())
    {
        const SVFValue* instruction = stmt->getValue();
        bool a = modification->queryIfFirstDefinition(instruction);

        if (a == true)
        {
            std::string inststring = instruction->toString();
            std ::cout << inststring << std::endl;
            modification->addNewCodeSnippetAfter(instruction, "int m = 1;");
            modification->replace(instruction, "int n = 2;");
        }

        //   std::string name = instruction->getName();
        //   std::cout << name << std::endl;
        if (const BranchStmt* branch = SVFUtil::dyn_cast<BranchStmt>(stmt))
        {
            std::string brstring = branch->getValue()->toString();
            SVFVar* branchVar = const_cast<SVFVar*>(branch->getBranchInst());
            SVFValue* branchValue =
                const_cast<SVFValue*>(branchVar->getValue());
            std::string location = branchValue->getSourceLoc();
            modification->deleteEitherBranch(branchValue,true);
            if (location == "")
            {
                break;
            }
            std::string::size_type pos = location.find("\"ln\":");
            int num = std::stoi(
                location.substr(pos + 5, location.find(",") - pos - 5));
            SVFVar* conditionVar = const_cast<SVFVar*>(branch->getCondition());
            std::string conditionstring = conditionVar->getValue()->toString();
            std::size_t icmpPos = conditionstring.find("icmp");
            if (icmpPos != std::string::npos)
            {
                std::size_t spacePos = conditionstring.find(" ", icmpPos);
                std::size_t nextSpacePos =
                    conditionstring.find(" ", spacePos + 1);
                std::string operation = conditionstring.substr(
                    spacePos + 1, nextSpacePos - spacePos - 1);
                std::vector<std::string> parameters = {};
                pos = conditionstring.find("\"fl\": \"");
                if (pos == std::string::npos)
                {
                    return;
                }
                std::string srcpath = conditionstring.substr(
                    pos + 7, conditionstring.find("\" }") - pos - 7);
                std ::cout << srcpath << std::endl;
                lightAnalysis->findNodeOnTree(num, branch_order, operation,
                                              parameters, srcpath);
            }
        }
    }
    // 调用Modification的析构函数
}

int main(int argc, char** argv)
{

    std::vector<std::string> moduleNameVec;
    moduleNameVec = OptionBase::parseOptions(
        argc, argv, "Tool to transform your code automatically",
        "[options] <input-bitcode...>");

    if (Options::WriteAnder() == "ir_annotator")
    {
        LLVMModuleSet::preProcessBCs(moduleNameVec);
    }

    if (SOURCEPATH().empty())
    {
        std::cerr << "You should specify the path of source code!" << std::endl;
        std::exit(1);
    }

    SVFModule* svfModule = LLVMModuleSet::buildSVFModule(moduleNameVec);
    SVF::SVFIRBuilder builder(svfModule);
    auto pag = builder.build();
    assert(pag && "pag cannot be nullptr!");
    ICFG* icfg = pag->getICFG();
    auto str = SOURCEPATH();
    auto modification = new SVF::Modification(str);
    for (const auto& it : *icfg)
    {
        const ICFGNode* node = it.second;
        traverseOnSVFStmt(node, modification);
    }
    // auto str = SOURCEPATH();
    // auto modification = new SVF::Modification(str);
    // modification->setHoleFilling(1, "a");
    delete modification;
    return 0;
}
