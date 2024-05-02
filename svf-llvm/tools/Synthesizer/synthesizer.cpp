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

void traverseOnSVFStmt(const ICFGNode* node)
{
    auto str = SOURCEPATH();
    auto lightAnalysis = new LightAnalysis(str);
    for (const SVFStmt* stmt : node->getSVFStmts())
    {
        std::string stmtstring = stmt->getValue()->toString();
        if (const BranchStmt* branch = SVFUtil::dyn_cast<BranchStmt>(stmt))
        {
            std::string brstring = branch->getValue()->toString();
            SVFVar* branchVar = const_cast<SVFVar*>(branch->getBranchInst());
            SVFValue* branchValue =
                const_cast<SVFValue*>(branchVar->getValue());
            std::string location = branchValue->getSourceLoc();
            if (location == "")
            {
                break;
            }

            std::string::size_type pos = location.find("\"ln\":");
            unsigned int num = std::stoi(
                location.substr(pos + 5, location.find(",") - pos - 5));
            SVFVar* conditionVar = const_cast<SVFVar*>(branch->getCondition());
            std::string conditionstring = conditionVar->getValue()->toString();
            //   %5 = icmp slt i32 %4, 0, !dbg !17 { "ln": 7, "cl": 11, "fl":
            //   "test1.c" }
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
                std::string srcpath = conditionstring.substr(
                    pos + 7, conditionstring.find("\" }") - pos - 7);
                lightAnalysis->findNodeOnTree(num, branch_order, operation,
                                              parameters, srcpath);
            }
        }

        else if (const AddrStmt* addr = SVFUtil::dyn_cast<AddrStmt>(stmt))
        {
            std::string addrstring = addr->getValue()->toString();
        }
        else if (const BinaryOPStmt* binary =
                     SVFUtil::dyn_cast<BinaryOPStmt>(stmt))
        {
            std::string bistring = binary->getValue()->toString();
        }
        else if (const CmpStmt* cmp = SVFUtil::dyn_cast<CmpStmt>(stmt))
        {
            std::string cmpstring = cmp->getValue()->toString();
        }
        else if (const LoadStmt* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            std::string loadstring = load->getValue()->toString();
        }
        else if (const StoreStmt* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            std ::string storestring = store->getValue()->toString();
        }
        else if (const CopyStmt* copy = SVFUtil::dyn_cast<CopyStmt>(stmt))
        {
            std::string copystring = copy->getValue()->toString();
        }
        else if (const GepStmt* gep = SVFUtil::dyn_cast<GepStmt>(stmt))
        {
            std ::string gepstring = gep->getValue()->toString();
        }
        else if (const SelectStmt* select = SVFUtil::dyn_cast<SelectStmt>(stmt))
        {
            std ::string selectstring = select->getValue()->toString();
        }
        else if (const PhiStmt* phi = SVFUtil::dyn_cast<PhiStmt>(stmt))
        {
            std::string phistring = phi->getValue()->toString();
        }
        else if (const RetPE* retPE = SVFUtil::dyn_cast<RetPE>(stmt))
        {
            std::string retstring = retPE->getValue()->toString();
        }
        /*    else if (const CallPE* callPE = SVFUtil::dyn_cast<CallPE>(stmt))
           {
               std::string callstring = callPE->getValue()->toString();
               std::cout << callstring << std::endl;
               CallICFGNode* callNode =
                   const_cast<CallICFGNode*>(callPE->getCallSite());
               std::string callinfo = callNode->toString();
               SVFInstruction* cs =
                   const_cast<SVFInstruction*>(callNode->getCallSite());
               std::string m = cs->getSourceLoc();
               std::string::size_type pos = m.find("\"ln\":");
               unsigned int num =
                   std::stoi(m.substr(pos + 5, m.find(",") - pos - 5));
               std::regex re("@(\\w+)\\((.+)\\)");
               std::smatch match;
               std::string functionName;
               std::vector<std::string> parameters;
               if (std::regex_search(callstring, match, re) && match.size() > 2)
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
               lightAnalysis->findNodeOnTree(num, call_order, functionName,
                                             parameters);
           } */
    }
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

    // 拿 svf value 有两种办法，一是从 svfmodule 拿到 svffunction 再拿到
    // basicblock 最后拿到一个 instruction list，然后你一个一个遍历筛选需要的
    // instruction，对于 call instruction 你可以用 getOperand 拿到它的参数
    // 拿到basicblock:
    auto str = SOURCEPATH();
    auto lightAnalysis = new LightAnalysis(str);
    for (const SVFFunction* F : svfModule->getFunctionSet())
    {
        std::string functionName = F->getName();
        std::string m = F->getSourceLoc();
        std::cout << m << std::endl;
        std ::cout << functionName << std::endl;
        for (const SVFBasicBlock* bb : F->getBasicBlockList())
        {
            for (const SVFInstruction* inst : bb->getInstructionList())
            {
                std::string inststring = inst->toString();
                std ::cout << inststring << std::endl;
                std::istringstream iss(inststring);
                std::vector<std::string> words;
                std::string word;
                while (iss >> word)
                {
                    words.push_back(word);
                }
                int flag = 0;

                if ((words[0] == "call" && words.size() >= 3 &&
                     words[2] != "@llvm.dbg.declare(metadata") ||
                    (words.size() >= 3 && words[2] == "call"))
                {
                    flag = 1;
                }
                if (flag == 0)
                {
                    continue;
                }

                if (flag == 1)
                {
                    std::cout << inststring << std::endl;
                    std::string m = inst->getSourceLoc();
                    //"{ \"ln\": 15, \"cl\": 12, \"fl\": \"test1.c\" }"
                    std::cout << m << std::endl;
                    std::string::size_type pos = m.find("\"ln\":");
                    unsigned int num =
                        std::stoi(m.substr(pos + 5, m.find(",") - pos - 5));
                    std::regex re("@(\\w+)\\((.+)\\)");
                    std::smatch match;
                    std::string functionName;
                    pos = m.find("\"fl\": \"");
                    std::string srcpath =
                        m.substr(pos + 7, m.find("\" }") - pos - 7);
                    std ::cout << srcpath << std::endl;
                    std::vector<std::string> parameters;
                    if (std::regex_search(inststring, match, re) &&
                        match.size() > 2)
                    {
                        functionName = match.str(1);
                        std::string parametersStr = match.str(2);
                        std::stringstream ss(parametersStr);
                        std::string parameter;
                        while (std::getline(ss, parameter, ','))
                        {
                            parameter.erase(0,
                                            parameter.find_first_not_of(' '));
                            parameter.erase(parameter.find_last_not_of(' ') +
                                            1);
                            parameters.push_back(parameter);
                        }
                    }
                    std::cout << functionName << std::endl;
                    if (functionName == "Smelt_parse_string")
                    {
                        printf("1\n");
                    }
                    lightAnalysis->findNodeOnTree(num, call_order, functionName,
                                                  parameters, srcpath);
                }
            }
        }
    }
    SVF::SVFIRBuilder builder(svfModule);
    auto pag = builder.build();
    assert(pag && "pag cannot be nullptr!");

    ICFG* icfg = pag->getICFG();

    for (const auto& it : *icfg)
    {
        const ICFGNode* node = it.second;
        traverseOnSVFStmt(node);
    }

    return 0;
}
