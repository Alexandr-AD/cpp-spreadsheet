#pragma once

#include "sheet.h"
#include "common.h"
#include "formula.h"

#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <stack>
#include <stdexcept>

// class Sheet;

class Cell : public CellInterface
{
public:
    explicit Cell(Sheet &sheet);
    ~Cell();

    // Cell(const Cell &other);
    // Cell &operator=(Cell rhs);

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;
    std::vector<Position> GetDependedCells() const;
    void InvalidateCache() const;
    // Функция для поиска циклических зависимостей
    bool HasCycle() const;

private:
    class Impl;
    /*     {
        public:
            virtual ~Impl() = default;
            virtual CellInterface::Value GetValue() const = 0;
            virtual std::string GetText() const = 0;
        }; */
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;
    /*     void InvalidateCache()
        {
            cache_.reset();
            for (auto ref : refs_)
            {
                if (ref.second->cache_.has_value())
                {
                    ref.second->InvalidateCache();
                }
            }
        } */

private:
    std::unique_ptr<Impl> impl_;
    Sheet &sheet_;
    // std::unordered_map<Position, Cell *> deps_; // список ячеек, на которые ссылается текущая
    // std::unordered_map<Position, Cell *> refs_; // список ячеек, ссылающихся на эту

    // mutable std::optional<FormulaInterface::Value> cache_; // перенести в определения FormulaImpl и тд.
};