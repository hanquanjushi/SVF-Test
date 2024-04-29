//
// Created by LiShangyu on 2024/3/5.
//

#include "SYN/LightAnalysis.h"

using namespace SVF;

LightAnalysis::LightAnalysis(const std::string& _srcPath)
{
    srcPath = _srcPath;
}

LightAnalysis::~LightAnalysis() {}

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
    unsigned int target_line;
    std::string functionName;
    std::vector<std::string> parameters;
};

void LightAnalysis::findNodeOnTree(unsigned int target_line, int order_number,
                                   const std::string& functionName,
                                   const std::vector<std::string>& parameters)
{
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, srcPath.c_str(), nullptr, 0, nullptr, 0, CXTranslationUnit_None);
    assert(unit && "unit cannot be nullptr!");
    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    if (order_number == 0)
    {
        VisitorData data{order_number, target_line, functionName, parameters};
        clang_visitChildren(cursor, &astVisitor, &data);
    }
    else if (order_number == 1)
    {
        VisitorData data{order_number, target_line, functionName};
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
    unsigned int target_line = data->target_line;
    if (line == target_line)
    {
        int order_number = data->order_number;
        //  printf("curCursor = %d\n", clang_getCursorKind(curCursor));
        switch (order_number)
        {
        case 0: {
          
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
                CXString cursor_name = clang_getCursorSpelling(curCursor);
                std::string current_function_name =
                    clang_getCString(cursor_name);
                if (current_function_name == functionName)
                {
                    std::cout << "Function name matches with the target "
                                 "function name.\n";

                    int params_size = params_type.size();
                    VisitorData data{params_size,
                                     static_cast<unsigned int>(params_size), "",
                                     params_type};
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
                VisitorData data{0, 0, operation, {}};
                clang_visitChildren(curCursor, &ifstmtVisitor, &data);
            }
            else if (static_cast<CXCursorKind>(
                         clang_getCursorKind(curCursor)) == CXCursor_WhileStmt)
            {
                VisitorData data{0, 0, operation, {}};
                clang_visitChildren(curCursor, &whilestmtVisitor, &data);
            }
            else if (static_cast<CXCursorKind>(
                         clang_getCursorKind(curCursor)) == CXCursor_ForStmt)
            {
                unsigned int childCount = 0;
                clang_visitChildren(curCursor, &countChildren, &childCount);
                VisitorData data{0, childCount, operation, {}};
                clang_visitChildren(curCursor, &forstmtVisitor, &data);
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
    unsigned int childnum = data->target_line;
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
                unsigned int numTokens = 0;
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
                unsigned int numTokens = 0;
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
            unsigned int numTokens = 0;
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
            unsigned int numTokens = 0;
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
            unsigned int numTokens = 0;
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
            unsigned int numTokens = 0;
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
        CXSourceRange ifrange = clang_getCursorExtent(cursor);
        printSourceRange(ifrange, "if");
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
    printf("m = %d\n", m);
     if (m == CXCursor_UnexposedExpr && data->order_number > 0 &&
        data->order_number != total)
    {
        int m = data->order_number - 1;
        m = total - m;
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
        int index = data->order_number - 1;
        if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                CXCursor_IntegerLiteral &&
            params[index] == "i32")
        {
            int m = data->order_number - 1;
            m = total - m;
            printf("%d param is int\n", m);
            CXSourceRange callRange = clang_getCursorExtent(cursor);
            printSourceRange(callRange, "param" + std::to_string(m));
        }
        else if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                     CXCursor_BinaryOperator &&
                 params[index] == "i32")
        {
            int m = index;
            m = total - m;
            printf("%d param is int\n", m);
            CXSourceRange callRange = clang_getCursorExtent(cursor);
            printSourceRange(callRange, "param" + std::to_string(m));
        }
        else if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
                     CXCursor_FloatingLiteral &&
                 params[index] == "float")
        {
            int m = index;
            m = total - m;
            printf("%d param is float\n", m);
            CXSourceRange callRange = clang_getCursorExtent(cursor);
            printSourceRange(callRange, "param" + std::to_string(m));
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
    int* count = (int*)clientData;

    if (static_cast<CXCursorKind>(clang_getCursorKind(cursor)) ==
        CXCursor_FloatingLiteral)
    {

        printf("%d param is float\n", *count);
    }
    return CXChildVisit_Continue;
}
enum CXChildVisitResult LightAnalysis::findIfElseScope(CXCursor cursor,
                                                       CXCursor parent,
                                                       CXClientData clientData)
{
    unsigned int* count = (unsigned int*)clientData;
    (*count)++;
    if (*count == 2)
    {
        CXSourceRange ifrange = clang_getCursorExtent(cursor);
        printSourceRange(ifrange, "if");
    }
    if (*count == 3)
    {
        CXSourceRange elseRange = clang_getCursorExtent(cursor);
        printSourceRange(elseRange, "else");
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
    unsigned int line, column;
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
    unsigned int* count = (unsigned int*)clientData;
    (*count)++;
    return CXChildVisit_Continue;
}