#include <UnivarExprTreeStringFactory.h>
#include <adt/bintree/BinaryTreeFastDepthIterator.h>

#include <sstream>

// ***  CONSTANTS  *** //
// ******************* //
template <typename NumericType> unsigned int const
    UnivarExprTreeStringFactory<NumericType>::OP_ADD_BASE_PRIORITY = 100;
template <typename NumericType> unsigned int const
    UnivarExprTreeStringFactory<NumericType>::OP_MUL_BASE_PRIORITY = 101;
template <typename NumericType> unsigned int const
    UnivarExprTreeStringFactory<NumericType>::OP_POW_BASE_PRIORITY = 102;
template <typename NumericType> unsigned int const
    UnivarExprTreeStringFactory<NumericType>::FUN_BASE_PRIORITY = 110;
template <typename NumericType> unsigned int const
    UnivarExprTreeStringFactory<NumericType>::BRACKET_BASE_PRIORITY = 1000000;
template <typename NumericType> char const
    UnivarExprTreeStringFactory<NumericType>::EXPRESSION_CHARS[] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', // Lower case
    'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', // letters
    'y', 'z',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', // Upper case
    'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', // letters
    'Y', 'Z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',           // Digits
    '.', ',',                                                   // Separators
    '+', '-', '*', '/', '^', '(', ')'                           // Operators
};

template <typename NumericType> std::string const
UnivarExprTreeStringFactory<NumericType>::FUNCTION_NAMES[] = {
    "exp",      "ln",       "sqrt",     "abs",      "cos",      "sin",
    "tan",      "acos",     "asin",     "atan",     "cosh",     "sinh",
    "tanh"
};




// ***  EXPRESSION TREE NODE STRING FACTORY INTERFACE  *** //
// ******************************************************* //
template <typename NumericType> IExprTreeNode<NumericType, NumericType> *
UnivarExprTreeStringFactory<NumericType>::make(std::string const &expr){
    // Initialize building process
    basePriority = 0;
    lastReadIsOpenPriorityOrSeparator = false;
    nodes.clear();
    ops.clear();

    // Do iterative building
    return makeIterative(prepareExpressionString(expr));
}




// ***  MAKE METHODS  *** //
// ********************** //
template <typename NumericType> IExprTreeNode<NumericType, NumericType> *
UnivarExprTreeStringFactory<NumericType>::makeIterative(
    std::string const &expr
){
    // Iterate over input expression until it has been entirely digested
    std::string subexpr(expr);
    while(!subexpr.empty()){
        // Obtain next symbol (tokenization)
        Symbol symbol = nextSymbol(subexpr);
        if(subexpr[0] == ',') subexpr = subexpr.substr(1); // Skip separator

        // Handle symbol depending on type
        switch(symbol.type){
            case UnivarExprTreeNode<NumericType>::SymbolType::OPERATOR: {
                if(isValidOpString(symbol.str)) handleOp(symbol);
                else {
                    std::stringstream ss;
                    ss  << "UnivarExprTreeStringFactory::makeRecursive "
                        << "received an unexpected operator: \""
                        << symbol.str << "\"";
                    throw HeliosException(ss.str());
                }
                break;
            }
            case UnivarExprTreeNode<NumericType>::SymbolType::NUMBER: {
                UnivarExprTreeNode<NumericType> *node =
                    new UnivarExprTreeNode<NumericType>();
                node->symbolType = symbol.type;
                node->num = (NumericType) stod(symbol.str);
                nodes.push_back(node);
                break;
            }
            case UnivarExprTreeNode<NumericType>::SymbolType::VARIABLE: {
                if(symbol.str == "-t"){ // Negative variable
                    UnivarExprTreeNode<NumericType> *right =
                        new UnivarExprTreeNode<NumericType>();
                    right->symbolType = symbol.type;
                    UnivarExprTreeNode<NumericType> *left =
                        new UnivarExprTreeNode<NumericType>();
                    left->symbolType =
                        UnivarExprTreeNode<NumericType>::SymbolType::NUMBER;
                    left->num = -1.0;
                    UnivarExprTreeNode<NumericType> *node =
                        new UnivarExprTreeNode<NumericType>(left, right);
                    node->symbolType =
                        UnivarExprTreeNode<NumericType>::SymbolType::OPERATOR;
                    node->op = UnivarExprTreeNode<NumericType>::OpType::OP_MUL;
                    nodes.push_back(node);
                }
                else { // Positive variable
                    UnivarExprTreeNode<NumericType> *node =
                        new UnivarExprTreeNode<NumericType>();
                    node->symbolType = symbol.type;
                    nodes.push_back(node);
                }
                break;
            }
            case UnivarExprTreeNode<NumericType>::SymbolType::FUNCTION: {
                if(isValidFunString(symbol.str)) handleFun(symbol);
                else{
                    std::stringstream ss;
                    ss  << "UnivarExprTreeStringFactory::makeRecursive "
                        << "received an unexpected function: \""
                        << symbol.str << "\"";
                    throw HeliosException(ss.str());
                }
                break;
            }
        }

        // Prepare for next iteration
        if( // Handle subexpr starts with pi
            subexpr.size() >= 2 && subexpr.substr(0, 2) == "pi" &&
            (subexpr.size() == 2 || !std::isalpha(subexpr[2]))
        )
            subexpr = subexpr.substr(2);
        else if( // Handle subexpr starts with e
            subexpr.size() >= 1 && subexpr.substr(0, 1) == "e" &&
            (subexpr.size() == 1 || !std::isalpha(subexpr[1]))
        )
            subexpr = subexpr.substr(1);
        else subexpr = subexpr.substr(symbol.str.size());
    }

    // Do the pending flushes after finishing the iterative process
    while(!ops.empty()) flush();
    UnivarExprTreeNode<NumericType> * tree = nodes[0];

    // Optimize the built Univariate Expression Tree
    doIPowOptimization(tree);

    // Return built Univariate Expression Tree
    return (IExprTreeNode<NumericType, NumericType> *) tree;
}


template <typename NumericType>
void UnivarExprTreeStringFactory<NumericType>::flush(){
    // Get, remove and validate next top operator
    Symbol &symbol = ops.back();
    if(symbol.type != UnivarExprTreeNode<NumericType>::SymbolType::OPERATOR){
        std::stringstream ss;
        ss  << "UnivarExprTreeStringFactory::flush detected an unexpected "
            <<  "operator: \"" << symbol.str << "\"";
        throw HeliosException(ss.str());
    }
    ops.pop_back();

    if(symbol.str == "("){ // Handle new tree building when top operator is (
        size_t const m = nodes.size(); // Num nodes
        if(
            nodes.size() > 1 && nodes[m-2]->isFunction() &&
            nodes[m-2]->getLeftChild() == nullptr
        ){ // Function opening
            UnivarExprTreeNode<NumericType> * input = nodes.back();
            nodes.pop_back();
            UnivarExprTreeNode<NumericType> * function = nodes.back();
            function->left = input;
        }
        // Common opening (both priority, function, and operator)
        basePriority = calcCloseBracketPriority();
        // Handle the particular atan2 as bivariate operator case
        if(!ops.empty() && ops.back().str == "atan2") flush();
    }
    else { // Build new tree by merging two top trees with operator as root
        UnivarExprTreeNode<NumericType> * right = nodes.back();
        nodes.pop_back();
        UnivarExprTreeNode<NumericType> * left = nodes.back();
        nodes.pop_back();
        UnivarExprTreeNode<NumericType> * newNode =
            new UnivarExprTreeNode<NumericType>(left, right);
        newNode->symbolType =
            UnivarExprTreeNode<NumericType>::SymbolType::OPERATOR;
        newNode->setOperator(symbol.str);
        nodes.push_back(newNode);
    }
}




// ***  POST-PROCESS METHODS  *** //
// ****************************** //
template <typename NumericType>
void UnivarExprTreeStringFactory<NumericType>::doIPowOptimization(
    UnivarExprTreeNode<NumericType> *_node
){
    BinaryTreeFastDepthIterator<UnivarExprTreeNode<NumericType>> btdi(_node);
    while(btdi.hasNext()){
        UnivarExprTreeNode<NumericType> *node = btdi.next();
        if(!node->isOperator()) continue; // Ignore non-operator nodes
        if(node->op != node->OP_POW) continue; // Ignore non-POW operators
        if(!node->right->isNumber()) continue; // Ignore if non-number exponent
        // Check whether exponent is integer or not
        NumericType dnum = node->right->num; // Decimal number
        NumericType inum = (int) dnum;
        NumericType dinum = (double) inum;
        if(dnum != dinum) continue; // Ignore if non-integer exponent
        node->op = node->OP_IPOW; // Make IPOW operator
    }
}




// ***  HANDLE METHODS  *** //
// ************************ //
template <typename NumericType>
void UnivarExprTreeStringFactory<NumericType>::handleOp(
    UnivarExprTreeStringFactory<NumericType>::Symbol const &symbol
){
    if(symbol.str == ")"){ // On close priority / function close operator
        while(ops.back().str != "(") flush();
        flush();
    }
    else if(symbol.str == "("){ // On open priority / function open operator
        ops.push_back(symbol);
        basePriority = calcOpenBracketPriority();
    }
    else{ // On typical operator (+-*/^atan2)
        unsigned int const opPriority = calcOpPriority(symbol); // Top priority
        // While top operator priority is >= handled operator priority, flush
        while((!ops.empty()) && calcOpPriority(ops.back()) >= opPriority)
            flush();
        ops.push_back(symbol); // Push handled operator priority
    }
}

template <typename NumericType>
void UnivarExprTreeStringFactory<NumericType>::handleFun(
    UnivarExprTreeStringFactory<NumericType>::Symbol const &symbol
){
    UnivarExprTreeNode<NumericType> *node =
        new UnivarExprTreeNode<NumericType>();
    node->symbolType = UnivarExprTreeNode<NumericType>::SymbolType::FUNCTION;
    nodes.push_back(node);
    if(symbol.str == "exp")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_exp;
    else if(symbol.str == "ln")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_ln;
    else if(symbol.str == "sqrt")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_sqrt;
    else if(symbol.str == "abs")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_abs;
    else if(symbol.str == "cos")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_cos;
    else if(symbol.str == "sin")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_sin;
    else if(symbol.str == "tan")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_tan;
    else if(symbol.str == "acos")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_acos;
    else if(symbol.str == "asin")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_asin;
    else if(symbol.str == "atan")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_atan;
    else if(symbol.str == "cosh")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_cosh;
    else if(symbol.str == "sinh")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_sinh;
    else if(symbol.str == "tanh")
        node->fun = UnivarExprTreeNode<NumericType>::FunType::f_tanh;
}




// ***  UTIL METHODS  *** //
// ********************** //
template <typename NumericType>
typename UnivarExprTreeStringFactory<NumericType>::Symbol
UnivarExprTreeStringFactory<NumericType>::nextSymbol(
    std::string const &expr
){
    // Flush until start ( of atan2 and skip first character if it is a comma
    if(expr[0] == ','){
        while(ops.back().str != "(") flush();
        lastReadIsOpenPriorityOrSeparator = true;
        return nextSymbol(expr.substr(1));
    }

    // Prepare next symbol finding
    char const c0 = expr[0];        // Initial character
    size_t const m = expr.size();   // Num. characters in expression

    // Find next symbol when initial character is a single-character operator
    if(
        c0 == '+'       ||      c0 == '-'       ||
        c0 == '*'       ||      c0 == '/'       ||
        c0 == '^'       ||
        c0 == '('       ||      c0 == ')'
    ){
        // Negative number if starts by - and [stacks are empty or (top op
        // is open parenthesis and num nodes < num non parenthesis ops)]
        bool const negativeNumber = c0 == '-' && (
            (nodes.empty() && ops.empty()) || lastReadIsOpenPriorityOrSeparator
        );
        if(!negativeNumber) { // Handle if operator, skip if negative number
            Symbol symbol;
            symbol.type =
                UnivarExprTreeNode<NumericType>::SymbolType::OPERATOR;
            symbol.str = c0;
            if(c0 == '(') lastReadIsOpenPriorityOrSeparator = true;
            else lastReadIsOpenPriorityOrSeparator = false;
            return symbol;
        }
    }

    // Find next symbol when initial character is a letter
    if(std::isalpha(c0)){
        size_t nonAlphaIdx = m;  // Index of the first non-letter character
        for(size_t i = 1 ; i < m ; ++i){  // Iteratively find first non-letter
            if(!std::isalpha(expr[i])){
                nonAlphaIdx = i;
                break;
            }
        }
        // Extract consecutive text token
        std::string symstr = expr.substr(0, nonAlphaIdx);
        if(symstr == "atan" && expr[nonAlphaIdx] == '2'){ // Handle atan2 case
            ++nonAlphaIdx;
            symstr = expr.substr(0, nonAlphaIdx);
        }
        // Check named numbers and variables
        if(expr[nonAlphaIdx] != '('){
            if( // If last char is an operator other than opening parentheses
                nonAlphaIdx==m ||  // Or there is nothing after named num/var
                expr[nonAlphaIdx] == '+'    || expr[nonAlphaIdx] == '-' ||
                expr[nonAlphaIdx] == '*'    || expr[nonAlphaIdx] == '/' ||
                expr[nonAlphaIdx] == '^'    || expr[nonAlphaIdx] == ')' ||
                expr[nonAlphaIdx] == ','  // Also a separator
            ){
                // Check named number : pi
                if(symstr == "pi"){
                    Symbol symbol;
                    symbol.type =
                        UnivarExprTreeNode<NumericType>::SymbolType::NUMBER;
                    symbol.str = stringFromNumber(M_PI);
                    lastReadIsOpenPriorityOrSeparator = false;
                    return symbol;
                }
                // Check named number : e
                if(symstr == "e"){
                    Symbol symbol;
                    symbol.type =
                        UnivarExprTreeNode<NumericType>::SymbolType::NUMBER;
                    symbol.str = stringFromNumber(M_E);
                    lastReadIsOpenPriorityOrSeparator = false;
                    return symbol;
                }
                // Check variable : t
                if(symstr == "t"){
                    Symbol symbol;
                    symbol.type =
                        UnivarExprTreeNode<NumericType>::SymbolType::VARIABLE;
                    symbol.str = symstr;
                    lastReadIsOpenPriorityOrSeparator = false;
                    return symbol;
                }
            }
            else{ // Error : Unexpected case
                std::stringstream ss;
                ss << "UnivarExprTreeStringFactory::nextSymbol found an "
                   << "unexpected character after operator/function name: "
                   << "'" << expr[nonAlphaIdx] << "'";
                throw HeliosException(ss.str());
            }
        }
        // Handle the particular case of atan2 as a binary operator (trick)
        if(symstr == "atan2"){
            Symbol symbol;
            symbol.type= UnivarExprTreeNode<NumericType>::SymbolType::OPERATOR;
            symbol.str = symstr;
            lastReadIsOpenPriorityOrSeparator = false;
            return symbol;
        }
        // Handle general case as a function
        lastReadIsOpenPriorityOrSeparator = false;
        return craftFunSymbol(symstr);
    }

    // Find next symbol when initial character is susceptible to be a number
    if(c0 == '-'){  // Handle negative numbers
        const char c1 = expr[1];
        if(c1 == 't'){  // Handle negative variable
            Symbol symbol;
            symbol.type =
                UnivarExprTreeNode<NumericType>::SymbolType::VARIABLE;
            symbol.str = "-t";
            lastReadIsOpenPriorityOrSeparator = false;
            return symbol;
        }
        if( (!std::isdigit(c1)) && c1 != '.'){
            std::stringstream ss;
            ss  << "UnivarExprTreeStringFactory::nextSymbol failed to parse "
                << "a negative number from the expression: \""
                << expr << "\"";
            throw HeliosException(ss.str());
        }
        Symbol symbol = craftNumSymbol(expr.substr(1));
        symbol.str = "-" + symbol.str;
        lastReadIsOpenPriorityOrSeparator = false;
        return symbol;
    }
    else if(c0 == '.' || std::isdigit(c0)){  // Handle non-negative numbers
        lastReadIsOpenPriorityOrSeparator = false;
        return craftNumSymbol(expr);
    }

    // Error: Unexpected case
    std::stringstream ss;
    ss  << "UnivarExprTreeStringFactory::nextSymbol cannot handle expression: "
        << "\"" << expr << "\"";
    throw HeliosException(ss.str());
}

template <typename NumericType> std::string
UnivarExprTreeStringFactory<NumericType>::prepareExpressionString(
    std::string const &expr
){
    // Clean expression
    std::string prep = cleanExpressionString(expr);

    // Replace (-( by (-1*(
    std::size_t pos;
    while( (pos = prep.find("(-(")) != std::string::npos ){
        prep.replace(pos, 3, "(-1*(");
    }

    // Replace by -1* if starts with -(
    if(prep.substr(0, 2) == "-("){
        prep = "-1*"+prep.substr(1);
    }

    // Return prepared expression string
    return prep;
}

template <typename NumericType> std::string
UnivarExprTreeStringFactory<NumericType>::cleanExpressionString(
    std::string const &expr
){
    // Prepare cleaning
    size_t const m = expr.size();
    size_t const n = sizeof(EXPRESSION_CHARS);
    std::string clean;

    // Do cleaning
    for(size_t i=0, j=0 ; i < m ; ++i){ // i-th expr char, j-th clean char
        char const c = expr[i];  // Current char
        // Check whether current char is an expression character or not
        bool isExpressionChar = false;
        for(size_t k = 0 ; k < n ; ++k){
            if(c == EXPRESSION_CHARS[k]){
                isExpressionChar = true;
                break;
            }
        }
        // Preserve expression characters only
        if(isExpressionChar){
            clean.push_back(c);
            ++j;
        }
    }

    // Return clean string
    return clean;
}

template <typename NumericType>
std::string UnivarExprTreeStringFactory<NumericType>::stringFromNumber(
    NumericType const x,
    std::streamsize const precision
){
    std::ostringstream oss;
    oss.precision(precision);
    oss << std::fixed << x;
    return oss.str();
}

template <typename NumericType>
typename UnivarExprTreeStringFactory<NumericType>::Symbol
UnivarExprTreeStringFactory<NumericType>::craftFunSymbol(
    std::string const &symstr
){
    // Prepare crafting of function-symbol
    size_t const m = sizeof(FUNCTION_NAMES); // Num. supported function names

    // Check symstr is a supported function name
    bool supported = false;
    for(size_t i = 0 ; i < m ; ++i){
        if(symstr == FUNCTION_NAMES[i]){
            supported = true;
            break;
        }
    }

    // Throw exception when function name is not supported
    if(!supported){
        std::stringstream ss;
        ss  << "UnivarExprTreeStringFactory::craftFunSymbol found an "
            << "unexpected function name: \"" << symstr << "\"";
        throw HeliosException(ss.str());
    }

    // Craft function-symbol
    Symbol symbol;
    symbol.type = UnivarExprTreeNode<NumericType>::SymbolType::FUNCTION;
    symbol.str = symstr;

    // Return crafted function-symbol
    return symbol;
}

template <typename NumericType>
typename UnivarExprTreeStringFactory<NumericType>::Symbol
UnivarExprTreeStringFactory<NumericType>::craftNumSymbol(
    std::string const &expr
){
    // Prepare crafting of numeric symbol
    size_t const m = expr.size();
    bool dotDigested = expr[0] == '.';
    size_t endOfNumberIdx = 1;

    // Handle numeric expression : find index of first nun-number character
    for(; endOfNumberIdx < m ; ++endOfNumberIdx){
        // Evaluate statements on c = ci = expr[i], i = endOfNumberIdx
        char const c = expr[endOfNumberIdx];
        bool const cIsNotDigit = !std::isdigit(c);
        bool const cIsDot = c == '.';
        bool const cIsNotDot = !cIsDot;
        if(cIsNotDigit && (cIsNotDot || dotDigested)) break; // End of number
        if(cIsDot) dotDigested = true; // Update dotDigested state
    }

    // Craft numeric symbol
    Symbol symbol;
    symbol.type = UnivarExprTreeNode<NumericType>::SymbolType::NUMBER;
    symbol.str = expr.substr(0, endOfNumberIdx);

    // Return crafted numeric symbol
    return symbol;
}
