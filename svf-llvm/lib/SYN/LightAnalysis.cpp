//
// Created by LiShangyu on 2024/3/5.
//

#include "SYN/LightAnalysis.h"

using namespace SVF;

LightAnalysis::LightAnalysis(const std::string& _srcPath)
{
    srcPath = _srcPath;
}

Modification::Modification(const std::string& srcpath)
{
    this->srcFilePath = srcpath;
    for (const auto& entry : fs::recursive_directory_iterator(srcpath))
    {
        if (entry.is_regular_file())
        {
            auto path = entry.path();
            if (path.extension() == ".c" || path.extension() == ".h")
            {
                // 为每个.c和.h文件创建ReadWriteContext对象
                ReadWriteContext context;
                // 假设ReadWriteContext有一个接受文件路径的构造函数
                context.srcFilePath = path.string();
                // 给context.oracleText赋值
                std::ifstream file(context.srcFilePath);
                std::ostringstream ss;
                ss << file.rdbuf();
                context.oracleText = ss.str();
                context.workText = context.oracleText;
                // 将文件路径与ReadWriteContext对象存储到map中
                fileContextMap[path.string()] = context;
            }
        }
    }
}

LightAnalysis::~LightAnalysis() {}
Modification::~Modification()
{
    // 遍历 fileContextMap
    for (auto& pair : fileContextMap)
    {
        // 获取文件上下文和文件路径
        ReadWriteContext& context = pair.second;

        const std::string& filePath = pair.first;
        // 打开文件进行写入
        context.outFp = fopen((filePath + "_synthesized").c_str(), "w");
        if (context.outFp == nullptr)
        {
            // 打开文件失败，可能需要处理错误
            continue;
        }

        // 写入修改后的文本
        fwrite(context.workText.c_str(), sizeof(char), context.workText.size(),
               context.outFp);

        // 关闭文件
        fclose(context.outFp);
    }
}

void LightAnalysis::runOnSrc()
{
    //     test: srcPath temporarily represents the whole file path.
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, srcPath.c_str(), nullptr, 0, nullptr, 0, CXTranslationUnit_None);
    assert(unit && "unit cannot be nullptr!");
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    clang_visitChildren(cursor, &cursorVisitor, nullptr);
}

struct VisitorData
{
    int order_number;
    int target_line;
    int visit_time;
    std::string functionName;
    std::vector<std::string> parameters;
    std::vector<int> lines;
    std::vector<int> columns;
};

void LightAnalysis::findNodeOnTree(int target_line, int order_number,
                                   const std::string& functionName,
                                   const std::vector<std::string>& parameters,
                                   std::string srcpathstring)
{
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, (srcPath + srcpathstring).c_str(), nullptr, 0, nullptr, 0,
        CXTranslationUnit_None);
    if (srcpathstring[0] == '/')
    {
        return;
    }
    assert(unit && "unit cannot be nullptr!");
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    if (order_number == 0)
    {
        VisitorData data{order_number, target_line, 0, functionName,
                         parameters};
        clang_visitChildren(cursor, &astVisitor, &data);
    }
    else if (order_number == 1)
    {
        VisitorData data{order_number, target_line, 0, functionName};
        clang_visitChildren(cursor, &astVisitor, &data);
    }
    else if (order_number == 2)
    {
        VisitorData data{order_number, target_line, 0, functionName};
        clang_visitChildren(cursor, &astVisitor, &data);
    }
}

enum CXChildVisitResult LightAnalysis::astVisitor(CXCursor curCursor,
                                                  CXCursor parent,
                                                  CXClientData client_data)
{
    CXSourceLocation loc = clang_getCursorLocation(curCursor);
    unsigned int line, column;
    CXFile file;
    clang_getSpellingLocation(loc, &file, &line, &column, nullptr);
    VisitorData* data = static_cast<VisitorData*>(client_data);
    unsigned target_line = data->target_line;
    if (line == target_line)
    {

        int order_number = data->order_number;
        //  printf("curCursor = %d\n", clang_getCursorKind(curCursor));
        switch (order_number)
        {
        case 0: {
            //   int m =
            //   static_cast<CXCursorKind>(clang_getCursorKind(curCursor));
            //    printf("m = %d\n", m);
            if (static_cast<CXCursorKind>(clang_getCursorKind(curCursor)) ==
                CXCursor_CallExpr)
            {
                std::string functionName = data->functionName;
                std::vector<std::string> params_type;
                std::cout << "Function name: " << functionName << "\n";
                std::vector<std::string> parameters = data->parameters;
                for (auto& parameter : parameters)
                {
                    size_t pos = parameter.find(' ');
                    if (pos != std::string::npos)
                    {
                        params_type.push_back(parameter.substr(0, pos));
                    }
                    else
                    {
                        params_type.push_back(parameter);
                    }
                }
                for (auto& param : params_type)
                {
                    std::cout << "param: " << param << "\n";
                }
                CXString cursor_name = clang_getCursorSpelling(curCursor);
                std::string current_function_name =
                    clang_getCString(cursor_name);

                if (current_function_name == functionName ||
                    current_function_name == "__builtin__" + functionName)
                {
                    std::cout << "Function name matches with the target "
                                 "function name.\n";

                    int params_size = params_type.size();
                    VisitorData data{params_size, static_cast<int>(params_size),
                                     0, "", params_type};
                    clang_visitChildren(curCursor, &callVisitor, &data);
                    CXSourceRange callRange = clang_getCursorExtent(curCursor);
                    printSourceRange(callRange, functionName);
                    CXString var_name = clang_getCursorSpelling(parent);
                    if (clang_getCursorKind(parent) == CXCursor_VarDecl)
                    {
                        std::cout << "Variable " << clang_getCString(var_name)
                                  << " is first defined here.\n";
                    }
                    auto temp_parent = parent;
                    while (static_cast<CXCursorKind>(clang_getCursorKind(
                               temp_parent)) != CXCursor_CompoundStmt &&
                           static_cast<CXCursorKind>(clang_getCursorKind(
                               temp_parent)) != CXCursor_FunctionDecl)
                    {
                        temp_parent =
                            clang_getCursorSemanticParent(temp_parent);
                    }
                    callRange = clang_getCursorExtent(temp_parent);
                    printSourceRange(callRange, "scope_where_call_is");
                }
                clang_disposeString(cursor_name);
            }
            break;
        }
        case 1: {
            std::string operation = data->functionName;
            if (static_cast<CXCursorKind>(clang_getCursorKind(curCursor)) ==
                CXCursor_IfStmt)
            {
                VisitorData data{0, 0, 0, operation, {}};

                clang_visitChildren(curCursor, &ifstmtVisitor, &data);
            }
            else if (static_cast<CXCursorKind>(
                         clang_getCursorKind(curCursor)) == CXCursor_WhileStmt)
            {
                VisitorData data{0, 0, 0, operation, {}};
                clang_visitChildren(curCursor, &whilestmtVisitor, &data);
            }
            else if (static_cast<CXCursorKind>(
                         clang_getCursorKind(curCursor)) == CXCursor_ForStmt)
            {
                int childCount = 0;
                clang_visitChildren(curCursor, &countChildren, &childCount);
                VisitorData data{0, childCount, 0, operation, {}};
                clang_visitChildren(curCursor, &forstmtVisitor, &data);
            }
            break;
        }
        case 2: {
            if (static_cast<CXCursorKind>(clang_getCursorKind(curCursor)) ==
                CXCursor_FunctionDecl)
            {
                std::string functionName = data->functionName;
                std::cout << "Function name: " << functionName << "\n";
                CXString cursor_name = clang_getCursorSpelling(curCursor);
                std::string current_function_name =
                    clang_getCString(cursor_name);
                if (current_function_name == functionName ||
                    current_function_name == "__builtin__" + functionName)
                {
                    std::cout << "Function name matches with the target "
                                 "function name.\n";
                    CXSourceRange defineRange =
                        clang_getCursorExtent(curCursor);
                    printSourceRange(defineRange,
                                     "scope where function defined is");
                }
                clang_disposeString(cursor_name);
            }
            break;
        }
        }
    }
    return CXChildVisit_Recurse;
}

enum CXChildVisitResult LightAnalysis::forstmtVisitor(CXCursor cursor,
                                                      CXCursor parent,
                                                      CXClientData clientData)
{
    VisitorData* data = static_cast<VisitorData*>(clientData);
    int childnum = data->target_line;
    if (childnum == 4)
    {
        data->order_number = data->order_number + 1;
        int count = data->order_number;
        if (count == 2)
        {
            std::string operation = data->functionName;
            if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                CXCursor_BinaryOperator)
            {
                CXSourceRange binaryrange = clang_getCursorExtent(cursor);
                printSourceRange(binaryrange, "binaryoperation");
                CXToken* tokens = 0;
                unsigned numTokens = 0;
                CXSourceRange range = clang_getCursorExtent(cursor);
                CXTranslationUnit TU = clang_Cursor_getTranslationUnit(cursor);
                clang_tokenize(TU, range, &tokens, &numTokens);
                int flag = 0;
                if (numTokens > 1)
                {
                    CXString op_name = clang_getTokenSpelling(TU, tokens[1]);
                    const char* op_string = clang_getCString(op_name);
                    if (strcmp(op_string, "<") == 0 && operation == "slt")
                    {
                        std::cout << "find <" << std::endl;
                        flag = 1;
                    }
                    else if (strcmp(op_string, "<=") == 0 && operation == "sle")
                    {
                        std::cout << "find <=" << std::endl;
                        flag = 1;
                    }
                    else if (strcmp(op_string, ">") == 0 && operation == "sgt")
                    {
                        std::cout << "find >" << std::endl;
                        flag = 1;
                    }
                    else if (strcmp(op_string, ">=") == 0 && operation == "sge")
                    {
                        std::cout << "find >=" << std::endl;
                        flag = 1;
                    }
                    else if (strcmp(op_string, "+") == 0 && operation == "ne")
                    {
                        flag = 1;
                    }
                    else if (strcmp(op_string, "-") == 0 && operation == "ne")
                    {
                        flag = 1;
                    }
                }
                printf("flag = %d\n", flag);
            }
            else if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                     CXCursor_UnaryOperator)
            {
                CXSourceRange range = clang_getCursorExtent(cursor);
                CXToken* tokens = 0;
                unsigned numTokens = 0;
                CXTranslationUnit TU = clang_Cursor_getTranslationUnit(cursor);
                clang_tokenize(TU, range, &tokens, &numTokens);
                int flag = 0;
                if (numTokens > 1)
                {
                    CXString op_name = clang_getTokenSpelling(TU, tokens[0]);

                    const char* op_string = clang_getCString(op_name);
                    if (strcmp(op_string, "!") == 0 && operation == "ne")
                    {
                        std::cout << "find !" << std::endl;
                        flag = 1;
                    }
                }
                printf("flag = %d\n", flag);
            }
        }
        if (count == 4)
        {
            CXSourceRange forrange = clang_getCursorExtent(cursor);
            printSourceRange(forrange, "for");
        }
    }
    return CXChildVisit_Continue;
}

enum CXChildVisitResult LightAnalysis::whilestmtVisitor(CXCursor cursor,
                                                        CXCursor parent,
                                                        CXClientData clientData)
{
    VisitorData* data = static_cast<VisitorData*>(clientData);

    data->order_number = data->order_number + 1;
    int count = data->order_number;
    if (count == 1)
    {
        std::string operation = data->functionName;
        if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
            CXCursor_BinaryOperator)
        {
            CXSourceRange binaryrange = clang_getCursorExtent(cursor);
            printSourceRange(binaryrange, "binaryoperation");
            CXToken* tokens = 0;
            unsigned numTokens = 0;
            CXSourceRange range = clang_getCursorExtent(cursor);
            CXTranslationUnit TU = clang_Cursor_getTranslationUnit(cursor);
            clang_tokenize(TU, range, &tokens, &numTokens);
            int flag = 0;
            if (numTokens > 1)
            {
                CXString op_name = clang_getTokenSpelling(TU, tokens[1]);
                const char* op_string = clang_getCString(op_name);
                if (strcmp(op_string, "<") == 0 && operation == "slt")
                {
                    std::cout << "find <" << std::endl;
                    flag = 1;
                }
                else if (strcmp(op_string, "<=") == 0 && operation == "sle")
                {
                    std::cout << "find <=" << std::endl;
                    flag = 1;
                }
                else if (strcmp(op_string, ">") == 0 && operation == "sgt")
                {
                    std::cout << "find >" << std::endl;
                    flag = 1;
                }
                else if (strcmp(op_string, ">=") == 0 && operation == "sge")
                {
                    std::cout << "find >=" << std::endl;
                    flag = 1;
                }
                else if (strcmp(op_string, "+") == 0 && operation == "ne")
                {
                    flag = 1;
                }
                else if (strcmp(op_string, "-") == 0 && operation == "ne")
                {
                    flag = 1;
                }
            }
            printf("flag = %d\n", flag);
        }
        else if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                 CXCursor_UnaryOperator)
        {
            CXSourceRange range = clang_getCursorExtent(cursor);
            CXToken* tokens = 0;
            unsigned numTokens = 0;
            CXTranslationUnit TU = clang_Cursor_getTranslationUnit(cursor);
            clang_tokenize(TU, range, &tokens, &numTokens);
            int flag = 0;
            if (numTokens > 1)
            {
                CXString op_name = clang_getTokenSpelling(TU, tokens[0]);

                const char* op_string = clang_getCString(op_name);
                if (strcmp(op_string, "!") == 0 && operation == "ne")
                {
                    std::cout << "find !" << std::endl;
                    flag = 1;
                }
            }
            printf("flag = %d\n", flag);
        }
    }
    if (count == 2)
    {
        CXSourceRange whilerange = clang_getCursorExtent(cursor);
        printSourceRange(whilerange, "while");
    }

    return CXChildVisit_Continue;
}

enum CXChildVisitResult LightAnalysis::ifstmtVisitor(CXCursor cursor,
                                                     CXCursor parent,
                                                     CXClientData clientData)
{
    VisitorData* data = static_cast<VisitorData*>(clientData);

    data->order_number = data->order_number + 1;
    int count = data->order_number;
    if (count == 1)
    {
        std::string operation = data->functionName;
        if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
            CXCursor_BinaryOperator)
        {
            CXSourceRange binaryrange = clang_getCursorExtent(cursor);
            printSourceRange(binaryrange, "binaryoperation");
            CXToken* tokens = 0;
            unsigned numTokens = 0;
            CXSourceRange range = clang_getCursorExtent(cursor);
            CXTranslationUnit TU = clang_Cursor_getTranslationUnit(cursor);
            clang_tokenize(TU, range, &tokens, &numTokens);

            if (numTokens > 1)
            {
                CXString op_name = clang_getTokenSpelling(TU, tokens[1]);
                const char* op_string = clang_getCString(op_name);
                std::cout << "operator is " << op_string << std::endl;
                if (strcmp(op_string, "<") == 0 && operation == "slt")
                {
                    std::cout << "find <" << std::endl;
                }
                else if (strcmp(op_string, "<=") == 0 && operation == "sle")
                {
                    std::cout << "find <=" << std::endl;
                }
                else if (strcmp(op_string, ">") == 0 && operation == "sgt")
                {
                    std::cout << "find >" << std::endl;
                }
                else if (strcmp(op_string, ">=") == 0 && operation == "sge")
                {
                    std::cout << "find >=" << std::endl;
                }
            }
        }
        else if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                 CXCursor_UnaryOperator)
        {
            CXSourceRange range = clang_getCursorExtent(cursor);
            CXToken* tokens = 0;
            unsigned numTokens = 0;
            CXTranslationUnit TU = clang_Cursor_getTranslationUnit(cursor);
            clang_tokenize(TU, range, &tokens, &numTokens);
            int flag = 0;
            if (numTokens > 1)
            {
                CXString op_name = clang_getTokenSpelling(TU, tokens[0]);

                const char* op_string = clang_getCString(op_name);
                if (strcmp(op_string, "!") == 0 && operation == "ne")
                {
                    std::cout << "find !" << std::endl;
                    flag = 1;
                }
            }
            printf("flag = %d\n", flag);
        }
        CXSourceRange range = clang_getCursorExtent(parent);
        printSourceRange(range, "whole_if");
    }
    if (count == 2)
    {
        CXSourceRange ifrange = clang_getCursorExtent(cursor);
        printSourceRange(ifrange, "if");
        CXSourceRange range = clang_getCursorExtent(parent);
        printSourceRange(range, "whole_if");
    }
    if (count == 3)
    {
        CXSourceRange elserange = clang_getCursorExtent(cursor);
        printSourceRange(elserange, "else");
        CXSourceRange range = clang_getCursorExtent(parent);
        printSourceRange(range, "whole_if");
    }
    return CXChildVisit_Continue;
}

enum CXChildVisitResult LightAnalysis::callVisitor(CXCursor cursor,
                                                   CXCursor parent,
                                                   CXClientData clientData)
{
    VisitorData* data = static_cast<VisitorData*>(clientData);
    std::vector<std::string> params = data->parameters;
    int total = (int)(data->target_line);
    int m = static_cast<CXCursorKind>(clang_getCursorKind(cursor));
    //   printf("m = %d\n", m);
    int index = total - data->order_number;
    int visit_time = data->visit_time;
    data->visit_time++;
    if (m == CXCursor_UnexposedExpr && data->order_number > 0 &&
        visit_time != 0)
    {
        VisitorData m = {index, 0, 0, params[index], params};
        clang_visitChildren(cursor, &visitunexposed, &m);
        data->order_number--;
        return CXChildVisit_Continue;
    }
    if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) !=
            CXCursor_IntegerLiteral &&
        static_cast<CXCursorKind>(clang_getCursorKind(cursor)) !=
            CXCursor_BinaryOperator &&
        static_cast<CXCursorKind>(clang_getCursorKind(cursor)) !=
            CXCursor_FloatingLiteral)
    {
        return CXChildVisit_Continue;
    }
    if (data->order_number > 0)
    {

        if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                CXCursor_IntegerLiteral &&
            params[index] == "i32")
        {
            printf("%d param is int\n", index);
            CXSourceRange callRange = clang_getCursorExtent(cursor);
            printSourceRange(callRange, "param" + std::to_string(index));
        }
        else if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                     CXCursor_BinaryOperator &&
                 (params[index] == "i32" || params[index] == "i64"))
        {

            printf("%d param is int\n", index);
            CXSourceRange callRange = clang_getCursorExtent(cursor);
            printSourceRange(callRange, "param" + std::to_string(index));
        }
        else if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                     CXCursor_FloatingLiteral &&
                 params[index] == "float")
        {
            printf("%d param is float\n", index);
            CXSourceRange callRange = clang_getCursorExtent(cursor);
            printSourceRange(callRange, "param" + std::to_string(index));
        }
        else
        {
            printf("%d param not match\n", index);
        }
        data->order_number--;
    }
    return CXChildVisit_Continue;
}
enum CXChildVisitResult LightAnalysis::visitunexposed(CXCursor cursor,
                                                      CXCursor parent,
                                                      CXClientData clientData)
{
    VisitorData* data = static_cast<VisitorData*>(clientData);
    int index = data->order_number;
    if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
            CXCursor_FloatingLiteral &&
        data->parameters[index] == "float")
    {
        printf("%d param is float\n", index);
        CXSourceRange paramRange = clang_getCursorExtent(cursor);
        printSourceRange(paramRange, "param" + std::to_string(index));
    }
    else if ((static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                  CXCursor_DeclRefExpr ||
              static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                  CXCursor_StringLiteral) &&
             (data->parameters[index] == "float*" ||
              data->parameters[index] == "i32*" ||
              data->parameters[index] == "i8*"))
    {
        printf("%d param is pointer\n", index);
        CXSourceRange paramRange = clang_getCursorExtent(cursor);
        printSourceRange(paramRange, "param" + std::to_string(index));
    }
    else if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                 CXCursor_IntegerLiteral &&
             data->parameters[index] == "i32")
    {
        printf("%d param is int\n", index);
        CXSourceRange paramRange = clang_getCursorExtent(cursor);
        printSourceRange(paramRange, "param" + std::to_string(index));
    }

    return CXChildVisit_Recurse;
}
enum CXChildVisitResult LightAnalysis::findIfElseScope(CXCursor cursor,
                                                       CXCursor parent,
                                                       CXClientData clientData)
{
    //  int* count = (int*)clientData;
    //  (*count)++;
    VisitorData* data = static_cast<VisitorData*>(clientData);
    data->order_number++;
    int count = data->order_number;

    if (count == 1)
    {
        CXSourceRange ifcondrange = clang_getCursorExtent(cursor);
        CXSourceLocation startLoc = clang_getRangeStart(ifcondrange);
        CXSourceLocation endLoc = clang_getRangeEnd(ifcondrange);
        unsigned int startLine, startColumn, endLine, endColumn;
        clang_getSpellingLocation(startLoc, NULL, &startLine, &startColumn,
                                  NULL);
        clang_getSpellingLocation(endLoc, NULL, &endLine, &endColumn, NULL);
        data->parameters.push_back(std::to_string(startLine));
        data->parameters.push_back(std::to_string(startColumn));
        data->parameters.push_back(std::to_string(endLine));
        data->parameters.push_back(std::to_string(endColumn));
    }
    if (count == 2)
    {
        CXSourceRange ifrange = clang_getCursorExtent(cursor);
        CXSourceLocation startLoc = clang_getRangeStart(ifrange);
        CXSourceLocation endLoc = clang_getRangeEnd(ifrange);
        unsigned startLine, startColumn, endLine, endColumn;
        clang_getSpellingLocation(startLoc, NULL, &startLine, &startColumn,
                                  NULL);
        clang_getSpellingLocation(endLoc, NULL, &endLine, &endColumn, NULL);
        data->parameters.push_back(std::to_string(startLine));
        data->parameters.push_back(std::to_string(startColumn));
        data->parameters.push_back(std::to_string(endLine));
        data->parameters.push_back(std::to_string(endColumn));
    }
    if (count == 3)
    {
        CXSourceRange elseRange = clang_getCursorExtent(cursor);
        CXSourceLocation startLoc = clang_getRangeStart(elseRange);
        CXSourceLocation endLoc = clang_getRangeEnd(elseRange);
        unsigned startLine, startColumn, endLine, endColumn;
        clang_getSpellingLocation(startLoc, NULL, &startLine, &startColumn,
                                  NULL);
        clang_getSpellingLocation(endLoc, NULL, &endLine, &endColumn, NULL);
        data->parameters.push_back(std::to_string(startLine));
        data->parameters.push_back(std::to_string(startColumn));
        data->parameters.push_back(std::to_string(endLine));
        data->parameters.push_back(std::to_string(endColumn));
    }
    return CXChildVisit_Continue;
}

void LightAnalysis::printSourceRange(CXSourceRange range,
                                     const std::string& blockName)
{
    CXSourceLocation startLoc = clang_getRangeStart(range);
    CXSourceLocation endLoc = clang_getRangeEnd(range);
    unsigned startLine, startColumn, endLine, endColumn;
    clang_getSpellingLocation(startLoc, NULL, &startLine, &startColumn, NULL);
    clang_getSpellingLocation(endLoc, NULL, &endLine, &endColumn, NULL);
    std::cout << "The " << blockName << " scope starts from line " << startLine
              << ", column " << startColumn << ", and ends at line " << endLine
              << ", column " << endColumn << "\n";
}

enum CXChildVisitResult LightAnalysis::defineVisitor(CXCursor curCursor,
                                                     CXCursor parent,
                                                     CXClientData client_data)
{
    CXSourceLocation loc = clang_getCursorLocation(curCursor);
    unsigned int line, column;
    CXFile file;
    clang_getSpellingLocation(loc, &file, &line, &column, nullptr);
    VisitorData* data = static_cast<VisitorData*>(client_data);
    unsigned target_line = data->target_line;
    if (line == target_line)
    {
        if (clang_getCursorKind(curCursor) == CXCursor_VarDecl)
        {
            CXString var_name = clang_getCursorSpelling(curCursor);
            int childCount = 0;
            clang_visitChildren(curCursor, &countChildren, &childCount);
            if (childCount > 0)
            {
                std::cout << "Variable " << clang_getCString(var_name)
                          << " is first defined here.\n";
                data->order_number = 1;
            }
        }
    }
    return CXChildVisit_Recurse;
}

enum CXChildVisitResult LightAnalysis::cursorVisitor(CXCursor curCursor,
                                                     CXCursor parent,
                                                     CXClientData client_data)
{
    CXString current_display_name = clang_getCursorDisplayName(curCursor);
    // Allocate a CXString representing the name of the current cursor
    auto str = clang_getCString(current_display_name);
    std::cout << "Visiting element " << str << "\n";
    // Print the char* value of current_display_name
    // 获取源代码中的位置
    CXSourceLocation loc = clang_getCursorLocation(curCursor);
    unsigned line, column;
    CXFile file;
    clang_getSpellingLocation(loc, &file, &line, &column, nullptr);
    // 将位置转换为行号、列号和文件名
    // 打印行号、列号和元素名称
    std::cout << "Visiting element " << str << " at line " << line
              << ", column " << column << "\n";
    clang_disposeString(current_display_name);
    // Since clang_getCursorDisplayName allocates a new CXString, it must be
    // freed. This applies to all functions returning a CXString
    return CXChildVisit_Recurse;
}

enum CXChildVisitResult LightAnalysis::countChildren(CXCursor cursor,
                                                     CXCursor parent,
                                                     CXClientData clientData)
{
    if (clang_getCursorKind(cursor) == CXCursor_TypeRef)
    {
        return CXChildVisit_Continue;
    }
    int* count = (int*)clientData;
    (*count)++;
    return CXChildVisit_Continue;
}

bool Modification::queryIfFirstDefinition(const SVFValue* defInst)
{
    std::string location = defInst->getSourceLoc();
    if (location == "")
    {
        // printf("location is empty\n");
        return false;
    }
    std::string::size_type pos = location.find("\"ln\":");
    if (pos == std::string::npos)
    {
        // printf("location is empty\n");
        return false;
    }
    int target_line =
        std::stoi(location.substr(pos + 5, location.find(",") - pos - 5));

    int flag = 0;
    pos = location.find("\"file\": \"");
    if (pos == std::string::npos)
    {
        flag = 1;
        pos = location.find("\"fl\": \"");
        if (pos == std::string::npos)
        {
            printf("location is empty\n");
            return false;
        }
    }
    std::string srcpathstring;
    if (flag == 0)
    {
        srcpathstring =
            location.substr(pos + 9, location.find("\" }") - pos - 9);
    }
    else
    {
        srcpathstring =
            location.substr(pos + 7, location.find("\" }") - pos - 7);
    }
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, (srcFilePath + srcpathstring).c_str(), nullptr, 0, nullptr, 0,
        CXTranslationUnit_None);

    assert(unit && "unit cannot be nullptr!");
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    VisitorData data{0, target_line, 0, "", {}};
    clang_visitChildren(cursor, &LightAnalysis::defineVisitor, &data);
    if (data.order_number == 1)
    {
        return true;
    }
    return false;
}

void Modification::addNewCodeSnippet(const SVFValue* startInst,
                                     const SVFValue* endInst, std::string str)
{
    addNewCodeSnippetAfter(startInst, str);
}

void Modification::addNewCodeSnippetAfter(const SVFValue* startInst,
                                          std::string str)
{
    std::string location = startInst->getSourceLoc();
    if (location == "")
    {
        return;
    }
    std::string::size_type pos = location.find("\"ln\":");
    if (pos == std::string::npos)
    {
        return;
    }
    int target_line =
        std::stoi(location.substr(pos + 5, location.find(",") - pos - 5));

    int flag = 0;
    pos = location.find("\"file\": \"");
    if (pos == std::string::npos)
    {
        flag = 1;
        pos = location.find("\"fl\": \"");
        if (pos == std::string::npos)
        {
            return;
        }
    }
    std::string srcpathstring;
    if (flag == 0)
    {
        srcpathstring =
            location.substr(pos + 9, location.find("\" }") - pos - 9);
    }
    else
    {
        srcpathstring =
            location.substr(pos + 7, location.find("\" }") - pos - 7);
    }

    ReadWriteContext& context = fileContextMap[srcFilePath + srcpathstring];

    // Adjust the target line number based on lineOffsetMap
    int adjusted_target_line = target_line;
    for (const auto& offset_pair : context.lineOffsetMap)
    {
        if (offset_pair.first <= target_line)
        {
            adjusted_target_line += offset_pair.second;
        }
        else
        {
            break;
        }
    }

    std::stringstream ss(context.workText);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }
    // Insert the new code snippet after the adjusted target line
    if (adjusted_target_line < (int)(lines.size()))
    {
        lines.insert(lines.begin() + adjusted_target_line, str);
    }
    // 重新整合workText
    std::ostringstream os;
    for (const auto& line : lines)
    {
        os << line << "\n";
    }
    context.workText = os.str();

    // Create a new map to store the updated pairs
    std::map<int, int> newOffsetMap;

    // Update the lineOffsetMap for all lines after the adjusted target line
    for (const auto& offset_pair : context.lineOffsetMap)
    {
        if (offset_pair.first > adjusted_target_line)
        {
            // Insert a new pair with the modified key and the same value
            newOffsetMap[offset_pair.first + 1] = offset_pair.second;
        }
        else
        {
            // Keep the same pair if it is not after the adjusted target line
            newOffsetMap[offset_pair.first] = offset_pair.second;
        }
    }

    // Replace the old map with the new map
    context.lineOffsetMap = std::move(newOffsetMap);
    // Add a new offset for the inserted line
    context.lineOffsetMap[target_line] += 1;

    // Store the updated context back in the fileContextMap
    fileContextMap[srcFilePath + srcpathstring] = context;
}

void Modification::addNewCodeSnippetBefore(const SVFValue* startInst,
                                           std::string str)
{
    std::string location = startInst->getSourceLoc();
    if (location == "")
    {
        return;
    }
    std::string::size_type pos = location.find("\"ln\":");
    if (pos == std::string::npos)
    {
        return;
    }
    int target_line =
        std::stoi(location.substr(pos + 5, location.find(",") - pos - 5));

    int flag = 0;
    pos = location.find("\"file\": \"");
    if (pos == std::string::npos)
    {
        flag = 1;
        pos = location.find("\"fl\": \"");
        if (pos == std::string::npos)
        {
            return;
        }
    }
    std::string srcpathstring;
    if (flag == 0)
    {
        srcpathstring =
            location.substr(pos + 9, location.find("\" }") - pos - 9);
    }
    else
    {
        srcpathstring =
            location.substr(pos + 7, location.find("\" }") - pos - 7);
    }

    ReadWriteContext& context = fileContextMap[srcFilePath + srcpathstring];

    // Adjust the target line number based on lineOffsetMap
    int adjusted_target_line = target_line;
    for (const auto& offset_pair : context.lineOffsetMap)
    {
        if (offset_pair.first < target_line)
        {
            adjusted_target_line += offset_pair.second;
        }
        else
        {
            break;
        }
    }

    std::stringstream ss(context.workText);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }
    // Insert the new code snippet before the adjusted target line
    if (adjusted_target_line > 0)
    {
        lines.insert(lines.begin() + adjusted_target_line - 1, str);
    }
    // 重新整合workText
    std::ostringstream os;
    for (const auto& line : lines)
    {
        os << line << "\n";
    }
    context.workText = os.str();

    // Create a new map to store the updated pairs
    std::map<int, int> newOffsetMap;

    // Update the lineOffsetMap for all lines after the adjusted target line
    for (const auto& offset_pair : context.lineOffsetMap)
    {
        if (offset_pair.first >= adjusted_target_line)
        {
            // Insert a new pair with the modified key and the same value
            newOffsetMap[offset_pair.first + 1] = offset_pair.second;
        }
        else
        {
            // Keep the same pair if it is not after the adjusted target line
            newOffsetMap[offset_pair.first] = offset_pair.second;
        }
    }

    // Replace the old map with the new map
    context.lineOffsetMap = std::move(newOffsetMap);
    // Add a new offset for the inserted line
    context.lineOffsetMap[target_line - 1] += 1;

    // Store the updated context back in the fileContextMap
    fileContextMap[srcFilePath + srcpathstring] = context;
}

void Modification::replace(const SVFValue* inst, std::string str)
{
    std::string location = inst->getSourceLoc();
    if (location == "")
    {
        return;
    }
    std::string::size_type pos = location.find("\"ln\":");
    if (pos == std::string::npos)
    {
        return;
    }
    int target_line =
        std::stoi(location.substr(pos + 5, location.find(",") - pos - 5));

    int flag = 0;
    pos = location.find("\"file\": \"");
    if (pos == std::string::npos)
    {
        flag = 1;
        pos = location.find("\"fl\": \"");
        if (pos == std::string::npos)
        {
            return;
        }
    }
    std::string srcpathstring;
    if (flag == 0)
    {
        srcpathstring =
            location.substr(pos + 9, location.find("\" }") - pos - 9);
    }
    else
    {
        srcpathstring =
            location.substr(pos + 7, location.find("\" }") - pos - 7);
    }

    ReadWriteContext& context = fileContextMap[srcFilePath + srcpathstring];

    // Adjust the target line number based on lineOffsetMap
    int adjusted_target_line = target_line;
    for (const auto& offset_pair : context.lineOffsetMap)
    {
        if (offset_pair.first < target_line)
        {
            adjusted_target_line += offset_pair.second;
        }
        else
        {
            break;
        }
    }

    std::stringstream ss(context.workText);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }
    // Replace the content of the specified line with the new code snippet
    if (adjusted_target_line <= (int)(lines.size()))
    {
        lines[adjusted_target_line - 1] =
            str; // 注意这里的索引是从0开始的，所以需要减1
    }
    // 重新整合workText
    std::ostringstream os;
    for (const auto& line : lines)
    {
        os << line << "\n";
    }
    context.workText = os.str();
    // 注意：在替换操作中，我们不需要修改lineOffsetMap，因为行数没有变化
    // Store the updated context back in the fileContextMap
    fileContextMap[srcFilePath + srcpathstring] = context;
}

enum CXChildVisitResult LightAnalysis::branchVisitor(CXCursor curCursor,
                                                     CXCursor parent,
                                                     CXClientData client_data)
{
    CXSourceLocation loc = clang_getCursorLocation(curCursor);
    unsigned line, column;
    CXFile file;
    clang_getSpellingLocation(loc, &file, &line, &column, nullptr);
    VisitorData* data = static_cast<VisitorData*>(client_data);

    unsigned target_line = data->target_line;
    if (line == target_line)
    {
        // int m = static_cast<CXCursorKind>(clang_getCursorKind(curCursor));
        //  printf ("m = %d\n", m);
        if (static_cast<CXCursorKind>(clang_getCursorKind(curCursor)) ==
            CXCursor_IfStmt)
        {
            int childnum = 0;
            // childnum是3说明有else
            // 如果childnum是3，parent的location是整体范围，child1的location是if后面括号的范围，child2的location是if后面的{}的范围，child3的location是else后面的{}的范围
            // 如果childnum是2，parent的location是整体范围，child1的location是if后面括号的范围，child2的location是if后面的{}的范围
            std::vector<std::string> params;
            VisitorData data2{0, 0, 0, "", params};
            clang_visitChildren(curCursor, &findIfElseScope, &data2);
            CXSourceRange wholeifrange = clang_getCursorExtent(curCursor);
            CXSourceLocation startLoc = clang_getRangeStart(wholeifrange);
            CXSourceLocation endLoc = clang_getRangeEnd(wholeifrange);
            unsigned startLine, startColumn, endLine, endColumn;
            clang_getSpellingLocation(startLoc, NULL, &startLine, &startColumn,
                                      NULL);
            clang_getSpellingLocation(endLoc, NULL, &endLine, &endColumn, NULL);

            childnum = data2.order_number;
            data->order_number = childnum;
            data->lines.push_back(startLine);
            data->columns.push_back(startColumn);
            data->lines.push_back(endLine);
            data->columns.push_back(endColumn);

            if (childnum == 3)
            {
                for (int i = 0; i < 12; i = i + 2)
                {
                    data->lines.push_back(std::stoi(data2.parameters[i]));
                    data->columns.push_back(std::stoi(data2.parameters[i + 1]));
                }
            }
            else if (childnum == 2)
            {
                for (int i = 0; i < 8; i = i + 2)
                {
                    data->lines.push_back(std::stoi(data2.parameters[i]));
                    data->columns.push_back(std::stoi(data2.parameters[i + 1]));
                }
            }
        }
    }
    return CXChildVisit_Recurse;
}

void Modification::deleteCodeRange(int startLine, int startColumn, int endLine,
                                   int endColumn, std::string srcpathstring)
{
    // Assuming we have access to fileContextMap and srcFilePath similar to the
    // reference functions Assuming srcpathstring can be obtained in a similar
    // manner as the reference functions

    ReadWriteContext& context = fileContextMap[srcFilePath + srcpathstring];

    // Adjust the start and end line numbers based on lineOffsetMap
    int adjusted_start_line = startLine;
    int adjusted_end_line = endLine;
    for (const auto& offset_pair : context.lineOffsetMap)
    {
        if (offset_pair.first <= startLine)
        {
            adjusted_start_line += offset_pair.second;
        }
        if (offset_pair.first < endLine)
        {
            adjusted_end_line += offset_pair.second;
        }
    }

    std::stringstream ss(context.workText);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }

    // Check if the range is valid
    if (adjusted_start_line <= adjusted_end_line &&
        adjusted_start_line <= (int)(lines.size()) &&
        adjusted_end_line <= (int)(lines.size()))
    {
        // Delete the specified range of text
        for (int i = adjusted_start_line; i <= adjusted_end_line; ++i)
        {
            if (i == adjusted_start_line && startColumn > 1)
            {
                // If not deleting from the beginning of the line, preserve text
                // before startColumn
                lines[i - 1] = lines[i - 1].substr(0, startColumn - 1);
            }
            else if (i == adjusted_end_line &&
                     endColumn < (int)(lines[i - 1].length()))
            {
                // If not deleting to the end of the line, preserve text after
                // endColumn
                lines[i - 1] = lines[i - 1].substr(endColumn);
            }
            else
            {
                // Delete the entire line
                lines[i - 1].clear();
            }
        }
    }

    // 重新整合workText
    std::ostringstream os;
    for (const auto& line : lines)
    {
        os << line << "\n";
    }
    context.workText = os.str();

    // Store the updated context back in the fileContextMap
    fileContextMap[srcFilePath + srcpathstring] = context;
}
void Modification::insertNegation(int ifLine, int ifColumn,
                                  std::string srcpathstring)
{
    // Assuming we have access to fileContextMap and srcFilePath similar to the
    // reference functions Assuming srcpathstring can be obtained in a similar
    // manner as the reference functions
    ReadWriteContext& context = fileContextMap[srcFilePath + srcpathstring];

    // Adjust the start and end line numbers based on lineOffsetMap
    int adjusted_start_line = ifLine;

    for (const auto& offset_pair : context.lineOffsetMap)
    {
        if (offset_pair.first <= ifLine)
        {
            adjusted_start_line += offset_pair.second;
        }
    }
    std::stringstream ss(context.workText);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(ss, line))
    {
        lines.push_back(line);
    }

    // Check if the line is valid
    if (adjusted_start_line <= (int)(lines.size()))
    {
        // Insert '!' before the condition
        lines[adjusted_start_line - 1].insert(ifColumn, "!"); 
    }

    std::ostringstream os;
    for (const auto& line : lines)
    {
        os << line << "\n";
    }
    context.workText = os.str();

    fileContextMap[srcFilePath + srcpathstring] = context;
}
void Modification::deleteEitherBranch(const SVFValue* branchInst,
                                      bool condValue)
{
    std::string location = branchInst->getSourceLoc();
    if (location == "")
    {
        return;
    }
    std::string::size_type pos = location.find("\"ln\":");
    int target_line =
        std::stoi(location.substr(pos + 5, location.find(",") - pos - 5));
    pos = location.find("\"fl\": \"");
    if (pos == std::string::npos)
    {
        return;
    }
    std::string srcpathstring =
        location.substr(pos + 7, location.find("\" }") - pos - 7);
    std ::cout << srcpathstring << std::endl;
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, (srcFilePath + srcpathstring).c_str(), nullptr, 0, nullptr, 0,
        CXTranslationUnit_None);
    assert(unit && "unit cannot be nullptr!");
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    VisitorData data{0, target_line, 0, "", {}, {}, {}};
    clang_visitChildren(cursor, &LightAnalysis::branchVisitor, &data);
    int childnum = data.order_number;
    std::vector<int> lines = data.lines;
    std::vector<int> columns = data.columns;
    if (lines.size() == 0)
    {
        return;
    }
    if (childnum == 3)
    {
        // 有else分支
        if (condValue)
        {
            // 把branch cond 加上!()，然后把
            // if后面的{}和 else 这个词删了
            int ifStartLine = lines[4];
            int ifStartColumn = columns[4] + 1;
            int ifEndLine = lines[6];
            int ifEndColumn = columns[6];

            int ifConditionStartLine = lines[2];
            int ifConditionStartColumn = columns[2];

            insertNegation(ifConditionStartLine, ifConditionStartColumn,
                           srcpathstring);
            deleteCodeRange(ifStartLine, ifStartColumn, ifEndLine, ifEndColumn,
                            srcpathstring);
        }
        else
        {
            // 仅删除else分支
            int elseStartLine = lines[6];
            int elseStartColumn = columns[6];
            int endLine = lines[7];
            int endColumn = columns[7];
            deleteCodeRange(elseStartLine, elseStartColumn - 4, endLine,
                            endColumn, srcpathstring);
        }
    }
    else if (childnum == 2)
    {
        // 没有else分支
        if (condValue)
        {
            int startLine = lines[0];
            int startColumn = columns[0];
            int endLine = lines[1];
            int endColumn = columns[1];
            deleteCodeRange(startLine, startColumn, endLine, endColumn,
                            srcpathstring);
        }
        // 如果没有else且condValue为false，则不需要删除代码
    }
}

void Modification::deleteBranch(const SVFValue* branchInst)
{
    std::string location = branchInst->getSourceLoc();
    if (location == "")
    {
        return;
    }
    std::string::size_type pos = location.find("\"ln\":");
    int target_line =
        std::stoi(location.substr(pos + 5, location.find(",") - pos - 5));
    pos = location.find("\"fl\": \"");
    if (pos == std::string::npos)
    {
        return;
    }
    std::string srcpathstring =
        location.substr(pos + 7, location.find("\" }") - pos - 7);
    std ::cout << srcpathstring << std::endl;
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, (srcFilePath + srcpathstring).c_str(), nullptr, 0, nullptr, 0,
        CXTranslationUnit_None);
    assert(unit && "unit cannot be nullptr!");
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    VisitorData data{0, target_line, 0, "", {}};
    clang_visitChildren(cursor, &LightAnalysis::branchVisitor, &data);

    std::vector<int> lines = data.lines;
    std::vector<int> columns = data.columns;
    if (lines.size() == 0)
    {
        return;
    }
    int startLine = lines[0];
    int startColumn = columns[0];
    int endLine = lines[1];
    int endColumn = columns[1];
    deleteCodeRange(startLine, startColumn, endLine, endColumn, srcpathstring);
}

void Modification::setHoleFilling(int holeNumber, std::string varName)
{
    std::cout << holeNumber << std::endl;
}