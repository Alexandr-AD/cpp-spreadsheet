#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm> // Для std::max и std::distance
#include <functional>
#include <iostream>
#include <optional>

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text)
{
    if (!pos.IsValid())
    {
        throw InvalidPositionException("Invalid position"s);
    }
    auto cell = cells_.find(pos);
    if (cell == cells_.end())
    {
        cells_.emplace(pos, std::make_unique<Cell>(*this));
        cells_[pos]->Set(text);
    }
    else
    {
        if (!cells_.at(pos)->GetReferencedCells().empty())
        {
            cells_.at(pos)->InvalidateCache();
        }
        // Сохраняем указатель на оригинальную ячейку
        auto old_cell = std::move(cells_.at(pos));
        cells_.erase(pos);
        cells_.emplace(pos, std::make_unique<Cell>(*this));
        try
        {
            cells_[pos]->Set(text);
        }
        catch (const CircularDependencyException &)
        {
            // Если была попытка создать цикл, откатываем изменения
            cells_.erase(pos);
            cells_.emplace(pos, std::move(old_cell));
            throw;
        }
    }
    // if ((pos.row < cells_.size()) && (pos.col < cells_[pos.row].size()))
    // {
    // }
}

const CellInterface *Sheet::GetCell(Position pos) const
{
    if (!pos.IsValid())
    {
        throw InvalidPositionException("Invalid position"s);
    }
    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

CellInterface *Sheet::GetCell(Position pos)
{
    if (!pos.IsValid())
    {
        throw InvalidPositionException("Invalid position"s);
    }
    auto it = cells_.find(pos);
    return it == cells_.end() ? nullptr : it->second.get();
}

void Sheet::ClearCell(Position pos)
{
    if (!pos.IsValid())
    {
        throw InvalidPositionException("Invalid position"s);
    }
    // if (static_cast<size_t>(pos.row) < cells_.size() && static_cast<size_t>(pos.col) < cells_[pos.row].size())
    // {
    // cells_[pos.row][pos.col].reset();
    cells_.erase(pos);
    // }
}

Size Sheet::GetPrintableSize() const
{
    Size size{0, 0};
    for (const auto &[pos, cell_ptr] : cells_)
    {
        if (cell_ptr && !cell_ptr->GetText().empty())
        {
            size.rows = std::max(size.rows, pos.row + 1);
            size.cols = std::max(size.cols, pos.col + 1);
        }
    }

    return size;
}

void Sheet::PrintValues(std::ostream &output) const
{
    // Size size = GetPrintableSize();
    // for (int i = 0; i < size.rows; ++i)
    // {
    //     for (int j = 0; j < size.cols; ++j)
    //     {
    //         if (i < static_cast<int>(cells_.size()) && j < static_cast<int>(cells_[i].size()) && cells_[i][j])
    //         {
    //             auto value = cells_[i][j]->GetValue();
    //             std::visit([&output](auto &&arg)
    //                        {
    //                 if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, FormulaError>) {
    //                     output << "#ARITHM!";
    //                 } else {
    //                     output << arg;
    //                 } }, value);
    //         }
    //         if (j < size.cols - 1)
    //         {
    //             output << '\t';
    //         }
    //     }
    //     output << '\n';
    // }

    Size size = GetPrintableSize();
    for (int i = 0; i < size.rows; ++i)
    {
        for (int j = 0; j < size.cols; ++j)
        {
            Position pos{i, j};
            auto cell = GetCell(pos);
            if (cell)
            {
                CellInterface::Value value = cell->GetValue();
                std::visit(
                    [&output](const auto &arg)
                    {
                        if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, FormulaError>)
                        {
                            output << "#ARITHM!";
                        }
                        else
                        {
                            output << arg;
                        }
                    },
                    value);
            }
            if (j < size.cols - 1)
            {
                output << '\t';
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream &output) const
{
    Size size = GetPrintableSize();
    for (int i = 0; i < size.rows; ++i)
    {
        for (int j = 0; j < size.cols; ++j)
        {
            Position pos{i, j};
            auto cell = GetCell(pos);
            if (cell)
            {
                output << cell->GetText();
            }
            if (j < size.cols - 1)
            {
                output << "\t";
            }
            // else
            // {
            //     output << "\t";
            // }
            // if (j < size.rows - 1)
            // {
            //     output << "\t";
            // }
        }
        output << "\n";
    }
}

std::unique_ptr<SheetInterface> CreateSheet()
{
    return std::make_unique<Sheet>();
}