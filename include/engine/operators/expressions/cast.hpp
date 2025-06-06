#pragma once
#include "engine/operators/expressions/expression.hpp"
#include "kernels/cast_operators_kernel.hpp"
#include <duckdb/planner/expression/list.hpp>


namespace our {

class CastExpression : public Expression {
    std::unique_ptr<Expression> child;
    DataType childType;
public:
    CastExpression(duckdb::Expression &expr): Expression(expr) {
        exprType = ExpressionType::CAST;

        auto &castExpr = expr.Cast<duckdb::BoundCastExpression>();
        // castExpr.source_type
        child = Expression::createExpression(*castExpr.child);
        
        childType = child->returnType;
    }
    
    ExpressionResult evaluate(ExpressionArg& arg) {
        ExpressionResult result;
        child->table_idx = table_idx;

        cudaStream_t& stream = arg.batchs[0]->stream;
        auto childRes = child->evaluate(arg);
        auto data = childRes.result;
        // 
        if(returnType == childType) return childRes;
        if(returnType == DataType::STRING || childType == DataType::STRING)
            throw std::runtime_error("Not Implemented Yet: to cast non numericals");
        // 1. allocate memory        
        void* res;
        CUDA_CHECK( cudaMallocAsync(&res, childRes.batchSize * getDataTypeNumBytes(returnType), stream) );
        // 2. cast & cpy
        switch (childType)
        {
        case DataType::INT:
            switch (returnType) 
            {
            case DataType::FLOAT:
                YallaSQL::Kernel::launch_numerical_cast(static_cast<int*>(data), static_cast<float*>(res), childRes.batchSize, stream);
            break;
            case DataType::DATETIME:
                YallaSQL::Kernel::launch_numerical_cast(static_cast<int*>(data), static_cast<int64_t*>(res), childRes.batchSize, stream);
            break;
            default:
                break;
            }
        break;
        case DataType::FLOAT:
            switch (returnType)
            {
            case DataType::INT:
                YallaSQL::Kernel::launch_numerical_cast(static_cast<float*>(data), static_cast<int*>(res), childRes.batchSize, stream);
            break;
            case DataType::DATETIME:
                YallaSQL::Kernel::launch_numerical_cast(static_cast<float*>(data), static_cast<int64_t*>(res), childRes.batchSize, stream);
            break;
            default:
                break;
            }
        break;
        case DataType::DATETIME:
        switch (returnType)
            {
            case DataType::INT:
                YallaSQL::Kernel::launch_numerical_cast(static_cast<int64_t*>(data), static_cast<int*>(res), childRes.batchSize, stream);
            break;
            case DataType::FLOAT:
                YallaSQL::Kernel::launch_numerical_cast(static_cast<int64_t*>(data), static_cast<float*>(res), childRes.batchSize, stream);
            break;
            default:
                break;
            }
        break;
        
        default:
            break;
        }
        if(child->exprType != ExpressionType::BOUND_VALUE)
            CUDA_CHECK(cudaFreeAsync(data, stream));
        result.result = res;
        result.batchSize = childRes.batchSize;
        result.nullset = childRes.nullset;
        return result;
    }

};

}