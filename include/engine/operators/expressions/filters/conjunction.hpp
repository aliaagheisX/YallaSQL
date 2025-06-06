#pragma once
#include "engine/operators/expressions/expression.hpp"
#include "kernels/comparison_operators_kernel.hpp"

#include <duckdb/planner/expression/list.hpp>

namespace our {

    enum class ConjuntionType : __uint8_t {
        AND, OR
    };
    [[nodiscard]] inline ConjuntionType getConjuntionType(duckdb::Expression &expr) {
        switch(expr.type) {
        case duckdb::ExpressionType::CONJUNCTION_AND:
            return ConjuntionType::AND;  
        case duckdb::ExpressionType::CONJUNCTION_OR:
            return ConjuntionType::OR;  
        default:
            throw std::runtime_error("Expression Type Not Supported: " + expr.ToString());
        }
    }  

    [[nodiscard]] inline ConjuntionType getConjuntionType(duckdb::ExpressionType type) {
        switch(type) {
        case duckdb::ExpressionType::CONJUNCTION_AND:
            return ConjuntionType::AND;  
        case duckdb::ExpressionType::CONJUNCTION_OR:
            return ConjuntionType::OR;  
        default:
            throw std::runtime_error("Expression Type Not Supported in Conjunction Join: ");
        }
    }  

class ConjuntionExpression: public Expression {
    bool isneg, isjoin;
public:
    std::vector<std::unique_ptr<Expression>> children;
    ConjuntionType conjunction_type;

    ConjuntionExpression(duckdb::Expression &expr, bool isneg = false): Expression(expr), isneg(isneg) {
        exprType = ExpressionType::COMPARISON;
        conjunction_type = getConjuntionType(expr);

        isjoin = false;
        // get left & right child
        auto& castExpr = expr.Cast<duckdb::BoundConjunctionExpression>();

        children.reserve(castExpr.children.size());
        for(auto& child: castExpr.children) {
            children.push_back( Expression::createExpression(*child) );
        }
    } 
    ConjuntionExpression(duckdb::JoinCondition& cond): isneg(false) {
        exprType = ExpressionType::COMPARISON;
        conjunction_type = getConjuntionType(cond.comparison);

        isjoin = true;
        
        children.reserve(2);
        children.push_back(Expression::createExpression(*cond.left));
        children.push_back(Expression::createExpression(*cond.right));

        children[0]->table_idx = 0;
        children[1]->table_idx = 1;
    } 
    // if there's multiple individual expressions in filter -> and them
    ConjuntionExpression(std::unique_ptr<Expression> left, std::unique_ptr<Expression> right, bool isjoin = false): isneg(false), isjoin(isjoin) {
        exprType = ExpressionType::COMPARISON;
        conjunction_type = ConjuntionType::AND;

        children.reserve(2);
        children.push_back(std::move(left));
        children.push_back(std::move(right));
    }

    ExpressionResult evaluate(ExpressionArg& arg) {
        ExpressionResult result;
        
        cudaStream_t& stream = arg.batchs[0]->stream;
        
        ExpressionResult res_lhs = children[0]->evaluate(arg);
        ExpressionResult res_rhs = children[1]->evaluate(arg);
        size_t batchSize = std::max(res_rhs.batchSize, res_lhs.batchSize);

        bool* lhs = static_cast<bool*>(res_lhs.result);
        bool* rhs = static_cast<bool*>(res_rhs.result); 

        YallaSQL::Kernel::OperandType t_lhs = res_lhs.batchSize == 1 ? YallaSQL::Kernel::OperandType::SCALAR : YallaSQL::Kernel::OperandType::VECTOR;
        YallaSQL::Kernel::OperandType t_rhs = res_rhs.batchSize == 1 ? YallaSQL::Kernel::OperandType::SCALAR : YallaSQL::Kernel::OperandType::VECTOR;
        // Allocate result memory
        bool*  res;
        CUDA_CHECK(cudaMallocAsync(&res, batchSize, stream));
        

        switch (conjunction_type) {
        case ConjuntionType::AND:
            YallaSQL::Kernel::launch_conditional_operators<bool, YallaSQL::Kernel::ANDOperator>(rhs, lhs, t_rhs, t_lhs, res, batchSize, stream, isneg);
            break;
        case ConjuntionType::OR:
            YallaSQL::Kernel::launch_conditional_operators<bool, YallaSQL::Kernel::OROperator>(rhs, lhs, t_rhs, t_lhs, res, batchSize, stream, isneg);
            break;

        }


        CUDA_CHECK(cudaFreeAsync(lhs, stream));
        CUDA_CHECK(cudaFreeAsync(rhs, stream));
        
        result.batchSize = std::max(res_lhs.batchSize, res_rhs.batchSize);
        result.result = res;
        result.nullset = res_lhs.nullset;
        return result;
    }

};
}