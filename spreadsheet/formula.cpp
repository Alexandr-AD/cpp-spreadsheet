#include "formula.h"
#include <variant>

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <sstream>

using namespace std::literals;

std::ostream &operator<<(std::ostream &output, FormulaError fe)
{
    return output << fe.ToString();
}

namespace
{
    class Formula : public FormulaInterface
    {
    public:
        // Реализуйте следующие методы:
        explicit Formula(std::string expression)
        try
            : ast_(ParseFormulaAST(std::move(expression)))
        {
        }
        catch (const std::exception &exc)
        {
            throw FormulaException(exc.what());
        }

        Value Evaluate(const SheetInterface &sheet) const override
        {
            auto getval = [&sheet](Position pos) -> double
            {
                if (!pos.IsValid())
                    throw FormulaError(FormulaError::Category::Ref);
                const CellInterface *cell = sheet.GetCell(pos);
                if (!cell)
                    return 0.0;
                auto value = cell->GetValue();
                if (std::holds_alternative<double>(value))
                {
                    return std::get<double>(value);
                }
                else if (std::holds_alternative<FormulaError>(value))
                {
                    throw std::get<FormulaError>(value);
                }
                else if (std::holds_alternative<std::string>(value))
                {
                    const std::string &text = std::get<std::string>(value);
                    if (text.empty())
                    {
                        return 0.0;
                    }
                    try
                    {
                        size_t pos = 0;
                        double result = std::stod(text, &pos);
                        if (pos == text.size()) { // Убедимся, что вся строка была числом
                            return result;
                        } else {
                            throw FormulaError(FormulaError::Category::Value);
                        }
                    }
                    catch (const std::invalid_argument &)
                    {
                        throw FormulaError(FormulaError::Category::Value);
                    }
                    catch (const std::out_of_range &)
                    {
                        throw FormulaError(FormulaError::Category::Value);
                    }
                }
                else
                {
                    throw FormulaError(FormulaError::Category::Value);
                }
            };
            try
            {

                double result = ast_.Execute(getval);
                if (!std::isfinite(result))
                {
                    return FormulaError(FormulaError::Category::Arithmetic);
                }
                return result;
            }
            catch (const FormulaError &fe)
            {
                return fe;
            }
        }

        std::string GetExpression() const override
        {
            std::ostringstream out;
            ast_.PrintFormula(out);
            return out.str();
        }

        std::vector<Position> GetReferencedCells() const override
        {
            auto &cells = ast_.GetCells();
            if (cells.empty())
            {
                return {};
            }
            std::vector<Position> refCells;
            for (auto &cell : cells)
            {
                refCells.push_back(cell);
            }
            std::sort(refCells.begin(), refCells.end());
            refCells.erase(std::unique(refCells.begin(), refCells.end()), refCells.end());
            return refCells;
        }

    private:
        FormulaAST ast_;
    };
} // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression)
{
    return std::make_unique<Formula>(std::move(expression));
}