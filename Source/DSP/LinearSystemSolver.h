#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

template <std::size_t N>
using Matrix =
    std::array<std::array<double, N>, N>;

template <std::size_t N>
using Vector =
    std::array<double, N>;

template <std::size_t N>
bool solveLinearSystem(
    Matrix<N> matrix,
    Vector<N> rhs,
    Vector<N>& solution
) noexcept
{
    static_assert(N > 0);

    constexpr double pivotTolerance = 1.0e-12;

    // Forward elimination.
    for (
        std::size_t pivotColumn = 0;
        pivotColumn < N;
        ++pivotColumn
    )
    {
        std::size_t pivotRow = pivotColumn;

        double largestPivot =
            std::abs(
                matrix[pivotColumn][pivotColumn]
            );

        // Partial pivoting:
        // find the largest available value
        // in this column.
        for (
            std::size_t row = pivotColumn + 1;
            row < N;
            ++row
        )
        {
            const auto candidate =
                std::abs(
                    matrix[row][pivotColumn]
                );

            if (candidate > largestPivot)
            {
                largestPivot = candidate;
                pivotRow = row;
            }
        }

        // Matrix is singular or too close to singular
        // for this simple solver.
        if (largestPivot < pivotTolerance)
            return false;

        if (pivotRow != pivotColumn)
        {
            std::swap(
                matrix[pivotRow],
                matrix[pivotColumn]
            );

            std::swap(
                rhs[pivotRow],
                rhs[pivotColumn]
            );
        }

        const auto pivot =
            matrix[pivotColumn][pivotColumn];

        // Eliminate everything below the pivot.
        for (
            std::size_t row = pivotColumn + 1;
            row < N;
            ++row
        )
        {
            const auto factor =
                matrix[row][pivotColumn]
                / pivot;

            matrix[row][pivotColumn] = 0.0;

            for (
                std::size_t column = pivotColumn + 1;
                column < N;
                ++column
            )
            {
                matrix[row][column] -=
                    factor
                    * matrix[pivotColumn][column];
            }

            rhs[row] -=
                factor * rhs[pivotColumn];
        }
    }

    // Back substitution.
    for (
        int row = static_cast<int>(N) - 1;
        row >= 0;
        --row
    )
    {
        auto value =
            rhs[static_cast<std::size_t>(row)];

        for (
            std::size_t column =
                static_cast<std::size_t>(row) + 1;
            column < N;
            ++column
        )
        {
            value -=
                matrix[
                    static_cast<std::size_t>(row)
                ][column]
                * solution[column];
        }

        const auto diagonal =
            matrix[
                static_cast<std::size_t>(row)
            ][
                static_cast<std::size_t>(row)
            ];

        if (std::abs(diagonal) < pivotTolerance)
            return false;

        solution[
            static_cast<std::size_t>(row)
        ] =
            value / diagonal;
    }

    return true;
}