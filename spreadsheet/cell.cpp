#include "cell.h"
#include "common.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

class Cell::Impl
{
public:
    Impl(Sheet &sheet) : sheet_(sheet) {} //, deps_(), refs_() {}
    Impl(const Impl &other) : text_(other.text_),
                              sheet_(other.sheet_)
    {
        if (!deps_.empty() && !refs_.empty())
        {
            deps_ = other.deps_;
            refs_ = other.refs_;
        }
    }
    virtual CellInterface::Value GetValue() const = 0;
    virtual std::string GetText() const
    {
        return text_;
    }
    // virtual void Set(std::string text) {}
    virtual std::vector<Position> GetReferencedCells() const
    {
        if (refs_.empty())
        {
            return {};
        }
        std::vector<Position> refCells;
        refCells.reserve(refs_.size());
        for (auto ref : refs_)
        {
            refCells.push_back(ref.first);
        }
        return refCells;
    }
    bool HasCycle() const
    {
        std::unordered_set<const Impl *> visiting;
        std::unordered_set<const Impl *> visited;

        std::function<bool(const Impl *)> dfs = [&](const Impl *cell) -> bool
        {
            if (visiting.find(cell) != visiting.end())
            {
                return true; // Ячейка находится в процессе посещения, цикл обнаружен
            }
            if (visited.find(cell) != visited.end())
            {
                return false; // Ячейка уже была посещена и не является частью цикла
            }

            visiting.insert(cell);
            for (const auto &dep : cell->deps_)
            {
                if (dep.second && dep.second->impl_.get())
                {
                    if (dfs(dep.second->impl_.get()))
                    {
                        return true;
                    }
                }
            }
            visiting.erase(cell);
            visited.insert(cell);

            return false;
        };

        return dfs(this);
    }
    virtual void InvalidateCache() const = 0;

    void SetText(std::string text)
    {
        text_ = text;
    }
    virtual ~Impl() = default;

protected:
    std::unordered_map<Position, Cell *> &GetRefs()
    {
        return refs_;
    }
    std::unordered_map<Position, Cell *> &GetDeps()
    {
        return deps_;
    }

public:
    // std::unique_ptr<Impl> impl_;
    std::string text_;
    Sheet &sheet_;
    std::unordered_map<Position, Cell *> deps_; // список ячеек, на которые ссылается текущая
    std::unordered_map<Position, Cell *> refs_; // список ячеек, ссылающихся на эту

    mutable std::optional<FormulaInterface::Value> cache_; // перенести в определения FormulaImpl и тд.
};
class Cell::EmptyImpl : public Cell::Impl
{
public:
    EmptyImpl(Sheet &sheet) : Impl(sheet)
    {
        SetText("");
    }
    CellInterface::Value GetValue() const override { return CellInterface::Value(); }
    std::string GetText() const override { return ""; }
    std::vector<Position> GetReferencedCells() const override { return {}; }

    void InvalidateCache() const override
    {
        return;
    }
};

class Cell::TextImpl : public Cell::Impl
{
public:
    explicit TextImpl(Sheet &sheet, const std::string &text) : Impl(sheet) /*  text_(text) */
    {
        SetText(text);
    }
    CellInterface::Value GetValue() const override
    {
        if (!text_.empty() && text_.front() == ESCAPE_SIGN)
        {
            return CellInterface::Value(text_.substr(1)); // возвращаем без `'`
        }
        return CellInterface::Value(text_);
    }
    std::string GetText() const override
    {
        return text_;
    }
    std::vector<Position> GetReferencedCells() const override
    {
        return {};
    }
    void InvalidateCache() const override
    {
        return;
    }

    // private:
    // std::string text_;
};

class Cell::FormulaImpl : public Cell::Impl
{
public:
    explicit FormulaImpl(Sheet &sheet, const std::string &formula) : Impl(sheet), formula_(ParseFormula(formula))
    {
        SetText(formula);
    }
    FormulaImpl(const FormulaImpl &other) : Impl(*(static_cast<Impl *>(this))) {}
    CellInterface::Value GetValue() const override
    {
        auto val = formula_->Evaluate(sheet_);
        if (std::holds_alternative<double>(val))
        {
            return CellInterface::Value(std::get<double>(val));
        }
        else
        {
            return CellInterface::Value(std::get<FormulaError>(val));
        }
    }
    std::string GetText() const override
    {
        return FORMULA_SIGN + formula_->GetExpression();
    }
    std::vector<Position> GetReferencedCells() const override
    {
        return formula_->GetReferencedCells();
    }
    void InvalidateCache() const override
    {
        cache_.reset();
        for (auto ref : refs_)
        {
            if (ref.second->impl_->cache_.has_value())
            {
                ref.second->InvalidateCache();
            }
        }
    }

    // ~FormulaImpl()
    // {
    //     auto deps = GetDeps();
    //     for (auto dep : deps)
    //     {
    //     }
    // }

public:
    std::unique_ptr<FormulaInterface> formula_;
};

// Реализуйте следующие методы
Cell::Cell(Sheet &sheet) : impl_(std::make_unique<Cell::EmptyImpl>(sheet)), sheet_(sheet)
{
}

Cell::~Cell() = default;

void Cell::Set(std::string text)
{
    if (text.empty())
    {
        impl_ = std::make_unique<Cell::EmptyImpl>(sheet_);
    }
    else if (text.front() == ESCAPE_SIGN)
    {
        impl_ = std::make_unique<Cell::TextImpl>(sheet_, text);
    }
    // else if (text.front() == ESCAPE_SIGN)
    else if (text.front() != FORMULA_SIGN || text.size() <= 1)
    {
        impl_ = std::make_unique<Cell::TextImpl>(sheet_, text);
    }
    else
    {
        try
        {
            // Сохраняем текущее состояние для отката
            auto old_deps = impl_->deps_;
            auto old_refs = impl_->refs_;
            auto old_text = impl_->text_;
            
            impl_ = std::make_unique<Cell::FormulaImpl>(sheet_, text.substr(1));

            auto cells = static_cast<Cell::FormulaImpl *>(impl_.get())->GetReferencedCells();
            if (cells.empty())
            {
                return;
            }
            
            // Добавляем зависимости и проверяем на цикл для каждой
            for (auto pos : cells)
            {
                Cell *cell = static_cast<Cell *>(sheet_.GetCell(pos));
                if (!cell)
                {
                    // Если ячейка не существует, создаем её как пустую
                    sheet_.SetCell(pos, "");
                    cell = static_cast<Cell *>(sheet_.GetCell(pos));
                }
                impl_->deps_[pos] = cell;
                
                // Добавляем обратную ссылку
                cell->impl_->refs_[pos] = this;
                
                // Проверяем на цикл после добавления каждой зависимости
                if (impl_->HasCycle())
                {
                    // Если обнаружен цикл, откатываем изменения и бросаем исключение
                    impl_->deps_ = old_deps;
                    impl_->refs_ = old_refs;
                    impl_->text_ = old_text;
                    // Откатываем обратные ссылки
                    for (auto pos2 : cells)
                    {
                        Cell *cell2 = static_cast<Cell *>(sheet_.GetCell(pos2));
                        if (cell2)
                        {
                            cell2->impl_->refs_.erase(pos2);
                        }
                    }
                    throw CircularDependencyException("Circular dependency detected");
                }
            }
        }
        catch (const FormulaException &)
        {
            throw;
        }
    }
    // else
    // {
    //     impl_ = std::make_unique<Cell::TextImpl>(sheet_, text);
    // }
}

void Cell::Clear()
{
    impl_ = std::make_unique<Cell::EmptyImpl>(sheet_);
}

Cell::Value Cell::GetValue() const
{
    return impl_->GetValue();
}

std::string Cell::GetText() const
{
    return impl_->GetText();
}

void Cell::InvalidateCache() const
{
    impl_->InvalidateCache();
}

bool Cell::HasCycle() const
{
    return impl_->HasCycle();
}

std::vector<Position> Cell::GetReferencedCells() const
{
    if (dynamic_cast<Cell::EmptyImpl *>(impl_.get()))
    {
        return static_cast<Cell::EmptyImpl *>(impl_.get())->GetReferencedCells();
    }
    return impl_->GetReferencedCells();
}