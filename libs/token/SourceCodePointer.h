#pragma once

namespace Wasp
{

struct SourceCodePointer
{
    int index;

    int line_num;
    int column_num;

    SourceCodePointer() : index(0), line_num(1), column_num(1) {};

    void advance()
    {
        index++;
    }

    void retreat()
    {
        index--;
    }

    void increment_line_number()
    {
        line_num++;
    }

    void increment_column_number()
    {
        column_num++;
    }

    void decrement_column_number()
    {
        column_num--;
    }

    void reset_column_number()
    {
        column_num = 1;
    }
};

}

