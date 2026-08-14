#include <JuceHeader.h>

#include "../Source/DSP/LinearSystemSolver.h"

class LinearSystemSolverTests final
    : public juce::UnitTest
{
public:
    LinearSystemSolverTests()
        : juce::UnitTest(
            "Linear System Solver",
            "DSP"
        )
    {
    }

    void runTest() override
    {
        testTwoByTwoSystem();
        testThreeByThreeSystem();
        testSingularSystem();
    }

private:
    void testTwoByTwoSystem()
    {
        beginTest(
            "Solves 2x2 linear system"
        );

        // 2x + y = 5
        // x - y  = 1
        //
        // Solution:
        // x = 2
        // y = 1

        Matrix<2> matrix {{
            {{ 2.0,  1.0 }},
            {{ 1.0, -1.0 }}
        }};

        Vector<2> rhs {{
            5.0,
            1.0
        }};

        Vector<2> solution {};

        const auto success =
            solveLinearSystem(
                matrix,
                rhs,
                solution
            );

        expect(success);

        expectWithinAbsoluteError(
            solution[0],
            2.0,
            1.0e-10
        );

        expectWithinAbsoluteError(
            solution[1],
            1.0,
            1.0e-10
        );
    }

    void testThreeByThreeSystem()
    {
        beginTest(
            "Solves 3x3 linear system"
        );

        // x + y + z  = 6
        // 2x - y + z = 3
        // x + 2y - z = 2
        //
        // Solution:
        // x = 1
        // y = 2
        // z = 3

        Matrix<3> matrix {{
            {{ 1.0,  1.0,  1.0 }},
            {{ 2.0, -1.0,  1.0 }},
            {{ 1.0,  2.0, -1.0 }}
        }};

        Vector<3> rhs {{
            6.0,
            3.0,
            2.0
        }};

        Vector<3> solution {};

        const auto success =
            solveLinearSystem(
                matrix,
                rhs,
                solution
            );

        expect(success);

        expectWithinAbsoluteError(
            solution[0],
            1.0,
            1.0e-10
        );

        expectWithinAbsoluteError(
            solution[1],
            2.0,
            1.0e-10
        );

        expectWithinAbsoluteError(
            solution[2],
            3.0,
            1.0e-10
        );
    }

    void testSingularSystem()
    {
        beginTest(
            "Rejects singular system"
        );

        // Second equation is just twice the first:
        //
        // x + 2y = 3
        // 2x + 4y = 6

        Matrix<2> matrix {{
            {{ 1.0, 2.0 }},
            {{ 2.0, 4.0 }}
        }};

        Vector<2> rhs {{
            3.0,
            6.0
        }};

        Vector<2> solution {};

        const auto success =
            solveLinearSystem(
                matrix,
                rhs,
                solution
            );

        expect(!success);
    }
};

static LinearSystemSolverTests
    linearSystemSolverTests;