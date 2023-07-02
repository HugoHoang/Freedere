#include <iostream>
#include <clang-c/Index.h>
#include <vector>
#include <fstream>
#include <functional>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include "icecream.hpp"
#include <utility>
#include <cstdlib>
//sources: https://shaharmike.com/cpp/libclang/
//clang++ -lclang -I/usr/lib/llvm-14/include tool.cpp -o tool -g
// transitive pointer passing
// class member function leak


void findLeaks(std::string pathToSrc){
    std::string command = "./runvg.sh " + pathToSrc;
    std::system(command.c_str());
}

std::ostream& operator<<(std::ostream& stream, const CXString& str)
{
    stream << clang_getCString(str);
    clang_disposeString(str);
    return stream;
}


std::vector<std::vector<unsigned>> getLeakLocations(std::string pathToSrc){
    std::vector<std::vector<unsigned>> v;
    std::ifstream file("vg_analysis.txt");
    std::string line, 
                keyword = pathToSrc.substr(pathToSrc.find_last_of('/') + 1) + ':';
    unsigned i = 0;
    while(std::getline(file,line)){
        if(line.find("HEAP SUMMARY:") != std::string::npos)break;
    }
    while(std::getline(file, line)){
        if(line.length() == 0 && v.size() > 0) {
            i++;
            continue;
        }
        size_t pos = line.find(keyword), endPos = pos + keyword.length();
        if(pos != std::string::npos){
            if(v.size() == i) {
                v.push_back(std::vector<unsigned>(0));
            }
            unsigned locLength = line.find_last_of(')') - endPos;
            std::string loc = line.substr(endPos, locLength);
            v.at(i).push_back(stoi(loc));        
        }
    }
    std::cout<< std::endl;
    return v;
}
std::string getCursorName(CXCursor c){
    CXString str = clang_getCursorSpelling(c);
    std::string s = clang_getCString(str);
    clang_disposeString(str);
    return s;
}

std::string getCursorKind(CXCursor c){
    CXString str = clang_getCursorKindSpelling(clang_getCursorKind(c));
    std::string s = clang_getCString(str);
    clang_disposeString(str);
    return s;
}

std::string getCursorType(CXCursor c){
    CXString str = clang_getTypeSpelling(clang_getCursorType(c));
    std::string s = clang_getCString(str);
    clang_disposeString(str);
    return s;
}
std::string getCursorTypeKind(CXCursor c){
    CXString str = clang_getTypeKindSpelling(clang_getCursorType(c).kind);
    std::string s = clang_getCString(str);
    clang_disposeString(str);
    return s;
}

std::string getCursorResultType(CXCursor c){
    CXString str = clang_getTypeSpelling(clang_getCursorResultType(c));
    std::string s = clang_getCString(str);
    clang_disposeString(str);
    return s;
}

CXCursor getBinOpRef (std::vector<CXCursor> v ){
    for(int i = 0; i < v.size(); i++){
        if (getCursorKind(v.at(i)) == "VarDecl" || getCursorKind(v.at(i)) == "FieldDecl"){
            return(v.at(i));
        }
        if (getCursorKind(v.at(i)) == "BinaryOperator"){
            return(v.at(i+1));
        }
        if(getCursorKind(v.at(i)) == "CallExpr"){
            static CXCursor temp;
            clang_visitChildren(v.at(i), [](CXCursor c, CXCursor parent, CXClientData client_data){
                CXCursor t = clang_getCursorReferenced(c);
                if(getCursorKind(t) == "VarDecl" || getCursorKind(t) == "FieldDecl"){
                    temp = t;
                    return CXChildVisit_Break;
                }
                return CXChildVisit_Recurse;
            }, nullptr);
            return temp;
        }
        
    }
    return v.at(0);
}

unsigned getCursorLine(CXCursor c){
    CXFile f;
    unsigned row, col, of;
    clang_getSpellingLocation(clang_getCursorLocation(c), &f, &row, &col, &of);
    return row;
}

bool isAllocatorLeak(CXCursor originalDecl, CXCursor &funcCursor){
    while(getCursorKind(funcCursor) != "FunctionDecl" && getCursorKind(funcCursor) != "CXXMethod"){
        funcCursor = clang_getCursorSemanticParent(funcCursor);
    }      

    if(getCursorType(originalDecl) != getCursorResultType(funcCursor)){
        return false;
    }
    static CXCursor temp = originalDecl;
    static bool ptrIsReturned = false;
    clang_visitChildren(funcCursor, [](CXCursor c, CXCursor parent, CXClientData client_data){
        if(getCursorKind(c) != "ReturnStmt"){
            return CXChildVisit_Recurse;
        }
        clang_visitChildren(c,[](CXCursor c, CXCursor parent, CXClientData client_data){
            if(!clang_equalCursors(temp, clang_getCursorReferenced(c))) {
                return CXChildVisit_Recurse;
            }
            ptrIsReturned = true;
            return CXChildVisit_Break;
        }, nullptr);
        return CXChildVisit_Break;
    }, nullptr);
    return ptrIsReturned;
}
int main(int argc, char* argv[]){
    if(argc != 2){
        std::cout << "invalid number of arguments" << std::endl;
        return -1;
    }
    findLeaks(argv[1]);
    static std::vector<std::vector<unsigned>> leakLoc = getLeakLocations(argv[1]);
    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(index, argv[1] + 2, nullptr, 0, nullptr, 0, CXTranslationUnit_None);
    CXCursor root = clang_getTranslationUnitCursor(unit);
    static bool checkpoint = false;

    if(leakLoc.empty()) {
        std::cout<< "Vector is empty; no leaks found"<< std::endl;
        goto cleanup;

    }

    static std::vector<CXCursor> cursors;
    // -7 -6 -4 -3 0 3 4 6 9 a
    while(!leakLoc.empty()){
        clang_visitChildren(root,[](CXCursor c, CXCursor parent, CXClientData client_data){
            if(clang_Location_isFromMainFile(clang_getCursorLocation(c)) == 0) {
                return CXChildVisit_Recurse;
            }
            unsigned row = getCursorLine(c);
            
            if(!cursors.empty()){
                unsigned oldRow = getCursorLine(cursors.at(0));
                if(row != oldRow){
                    cursors.clear();
                }
            }
            cursors.push_back(c);
            std::string childKind = getCursorKind(c);
            if((childKind == "CXXNewExpr" || childKind == "CallExpr") && row == leakLoc.at(0).at(0)){
                
                CXCursor originalDecl = clang_getCursorReferenced(getBinOpRef(cursors));
                std::string originalDeclKind = getCursorKind(originalDecl);
                if(originalDeclKind == "FieldDecl"){
                    // unsigned pRow = getCursorLine(originalDecl);
                    CXCursor classCursor = clang_getCursorSemanticParent(originalDecl);
                    while(getCursorKind(classCursor) != "ClassDecl") classCursor = clang_getCursorSemanticParent(originalDecl);
                    std::cout << "Field member '" << getCursorName(originalDecl)  << "' of class '" << clang_getCursorSpelling(classCursor) << "'(line:" << getCursorLine(classCursor) << ") is causing a memory leak. Please delete it using its destructor.\n";
                    leakLoc.erase(leakLoc.begin());
                    return CXChildVisit_Break;
                }
                else if(originalDeclKind == "VarDecl"){
                    CXCursor funcCursor = originalDecl;
                    if(isAllocatorLeak(originalDecl, funcCursor)){
                        leakLoc.at(0).erase(leakLoc.at(0).begin());
                        return CXChildVisit_Break;
                    }
                    std::cout <<"Variable '" << getCursorName(originalDecl) <<"'(line:" << getCursorLine(originalDecl)<<") is causing a memory leak. Please free it inside the function '" << getCursorName(funcCursor)<<"'(line:"<<getCursorLine(funcCursor)<<").\n";
                    leakLoc.erase(leakLoc.begin());
                    return CXChildVisit_Break;
                }
                else if(originalDeclKind == "ParmDecl" && getCursorTypeKind(originalDecl) == "Pointer"){
                    leakLoc.at(0).erase(leakLoc.at(0).begin());
                    return CXChildVisit_Break;
                }
            } 
            return CXChildVisit_Recurse;
        }, nullptr);
    }

    cleanup:
    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);
    return 0;
}
