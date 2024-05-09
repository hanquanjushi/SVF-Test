//
// Created by LiShangyu on 2024/3/5.
//

#ifndef SVF_LIGHTANALYSIS_H
#define SVF_LIGHTANALYSIS_H
#include "SVFIR/SVFValue.h"
#include "SVFIR/SVFVariables.h"
#include "clang-c/Index.h"
#include <algorithm>
#include <assert.h>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace SVF
{
    
class ReadWriteContext
{

    friend class Modification;

private:
    // 以传过来的 Modification 对应文件的绝对地址为基础，为每一个 File
    // 都维持一个 context，最后一起 dump 到文件
    std::string srcFilePath;

    std::string oracleText;

    std::string workText;

    // std::unordered_map<>;

    FILE* inFp = nullptr;

    // 统一为在原来的文件名后面加个 "_synthesized"
    // 为新文件名，放在同路径上，e.g. "a.c" => "a_synthesized.c".
    FILE* outFp = nullptr;

    std::map<int, int> lineOffsetMap;

public:
};

class Modification
{

private:
    /// 看看需要存些啥。
    const ICFGNode* icfgNode;
    int _mType;
    std::string concreteReplacementStmt;
    std::map<std::string, ReadWriteContext> fileContextMap;

public:
    // TODO: Need some specific handling strategies for different types?
    enum MType
    {
        Uninitialized,
        Delete,
        Replace,
        New,
        HoleFilling,
    };

    Modification();
    Modification(const std::string&);
    Modification(MType, const std::string&, const ICFGNode*);
    Modification(MType, const std::string&);
    ~Modification();

    // 下面函数用来设置 Modification 的状态，你可以想想这些有哪些需要存在类里面
    // field 里的，
    /// Delete（不处理 switch）.
    // 把 branch 的其中一个分支删掉，保留另一个。
    void deleteEitherBranch(const SVFValue* branchInst, bool condValue);

    // 把 branch 所有分支全删掉，注意处理下 if else 情况。
    void deleteBranch(const SVFValue* branchInst);

    void deleteLoop(const SVFValue* inst);

    void deleteStmt(const SVFValue* inst);

    /// Replace.
    void replace(const SVFValue* inst, std::string str);

    /// New.
    // 从 startInst 到 endInst 直接选一行插入。
    // sourcepath是命令行传的.ll文件的路径
    void addNewCodeSnippet(std::string sourcepath, const SVFValue* startInst,
                           const SVFValue* endInst, std::string str);

    void addNewCodeSnippetAfter(std::string sourcepath,
                                const SVFValue* startInst, std::string str);

    void addNewCodeSnippetBefore(std::string sourcepath,
                                 const SVFValue* endInst, std::string str);

    /// HoleFilling.
    // 可以理解为 hole 就是 "$"" + "holeNumber"，比如 $1, $2, $3,
    // ...，直接字符串精准匹配，换成 varName。
    void setHoleFilling(int holeNumber, std::string varName);

    // 通过 inst 去找 ast 上这句话定义的那个变量名，然后精准替换。
    void setHoleFilling(int holeNumber, const SVFValue* varDefInst);
};

class LightAnalysis
{

private:
    // the path of the directory that the source codes locate.
    // if the path is not a file, then must end with "/".
    std::string srcPath;

    std::string directFilePath;

    // we can get this from debug info of possible changes.
    std::string exactFile;

    /// Maintain the modification of lines, we do not delete lines, only
    /// increase lines. E.g. Table: line    offset
    ///               15         2  (from line 15, we add 2 lines)
    ///               17         3  (from line 17, we add 3 lines)
    std::map<int, int> lineOffsetMap;

public:
    LightAnalysis();

    LightAnalysis(const std::string&);

    ~LightAnalysis();

    // TODO: not sure about SVFValue or SVFVar, and the return type.
    // this function is used to map the SVFValue to its corresponding AST node.
    void projection(const SVFValue*, const SVFVar*);

    static void printSourceRange(CXSourceRange range,
                                 const std::string& blockName);

    static enum CXChildVisitResult printAST(CXCursor cursor, CXCursor parent,
                                            CXClientData clientData);

    static enum CXChildVisitResult cursorVisitor(CXCursor cursor,
                                                 CXCursor parent,
                                                 CXClientData client_data);

    static enum CXChildVisitResult countChildren(CXCursor cursor,
                                                 CXCursor parent,
                                                 CXClientData clientData);

    static enum CXChildVisitResult forstmtVisitor(CXCursor cursor,
                                                  CXCursor parent,
                                                  CXClientData clientData);

    static enum CXChildVisitResult whilestmtVisitor(CXCursor cursor,
                                                    CXCursor parent,
                                                    CXClientData clientData);

    static enum CXChildVisitResult ifstmtVisitor(CXCursor cursor,
                                                 CXCursor parent,
                                                 CXClientData clientData);

    static enum CXChildVisitResult visitunexposed(CXCursor cursor,
                                                  CXCursor parent,
                                                  CXClientData clientData);

    static enum CXChildVisitResult findIfElseScope(CXCursor cursor,
                                                   CXCursor parent,
                                                   CXClientData clientData);

    static enum CXChildVisitResult callVisitor(CXCursor cursor, CXCursor parent,
                                               CXClientData clientData);

    static enum CXChildVisitResult astVisitor(CXCursor cursor, CXCursor parent,
                                              CXClientData client_data);

    void runOnSrc();

    void findNodeOnTree(unsigned int target_line, int order_number,
                        const std::string& functionName,
                        const std::vector<std::string>& parameters,
                        std::string srcpathstring);
    static int getLineFromSVFSrcLocString(const std::string&);

    static int getColumnFromSVFSrcLocString(const std::string&);

    static std::string getFileFromSVFSrcLocString(const std::string&);

    /// Only works for Intra, Call, Ret ICFGNodes.
    static const SVFInstruction* getInstFromICFGNode(const ICFGNode*);
};

} // namespace SVF

#endif // SVF_LIGHTANALYSIS_H
