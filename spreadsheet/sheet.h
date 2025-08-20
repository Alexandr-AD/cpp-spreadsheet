#pragma once

// #include "cell.h"
#include "common.h"

#include <functional>
#include <vector>
#include <memory>

class Cell;

class Sheet : public SheetInterface
{
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface *GetCell(Position pos) const override;
    CellInterface *GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream &output) const override;
    void PrintTexts(std::ostream &output) const override;

    // Можете дополнить ваш класс нужными полями и методами

private:
    // Хранит указатели на ячейки
    // std::vector<std::vector<std::unique_ptr<CellInterface>>> cells_;
    std::unordered_map<Position, std::unique_ptr<Cell>> cells_;
};