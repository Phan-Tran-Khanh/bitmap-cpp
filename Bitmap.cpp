#include <iostream>
#include <fstream>
#include <string>
using namespace std;

// bmp 24 bit depth

struct Header
{
    unsigned char signature[2];
    unsigned char fileSize[4];
    unsigned char reserved1[2];
    unsigned char reserved2[2];
    unsigned char offSet[4];
};

struct DIB
{
    unsigned char dibSize[4];
    uint32_t width;
    uint32_t height;
    unsigned char colorPlanes[2];
    unsigned char colorDepth[2];
    unsigned char compression[4];
    uint32_t pixelSize;
    unsigned char horizontalRes[4];
    unsigned char verticalRes[4];
    unsigned char colorNumber[4];
    unsigned char colorImportant[4];
};

struct Pixel
{
    unsigned char B;
    unsigned char G;
    unsigned char R;
    //unsigned char A;
    //bmp 32 bit depth => no padding 
};

struct Bitmap
{
    Header hd;
    DIB dib;
    Pixel **bmData;
};

Bitmap *readBitmap(string &filename)
{
    Bitmap *bm = nullptr;
    ifstream fi(filename, ios::in | ios::binary);
    if (fi.is_open())
    {
        bm = new Bitmap;
        fi.read((char *)&bm->hd, sizeof(bm->hd));
        if (bm->hd.signature[0] != 'B' || bm->hd.signature[1] != 'M')
        {
            delete bm;
            bm = nullptr;
        }
        else
        {
            fi.read((char *)&bm->dib, sizeof(bm->dib));
            uint32_t padding = (4 - (3 * bm->dib.width) % 4) % 4;
            bm->bmData = new Pixel *[bm->dib.height];
            if (bm->bmData != nullptr)
            {
                for (int64_t i = bm->dib.height - 1; i >= 0; --i)
                {
                    bm->bmData[i] = new Pixel[bm->dib.width];
                    if (bm->bmData[i] != nullptr)
                    {
                        for (uint32_t j = 0; j < bm->dib.width; ++j)
                            fi.read((char *)&bm->bmData[i][j], sizeof(bm->bmData[i][j]));
                        char temp = 0;
                        for (uint32_t j = 0; j < padding; ++j)
                            fi.read((char *)&temp, sizeof(temp));
                    }
                }
            }
        }
        fi.close();
    }
    return bm;
}

bool writeBitmap(string &filename, Bitmap *&bm, uint32_t &padding)
{
    ofstream fo(filename, ios::out | ios::binary);
    fo.write((char *)&bm->hd, sizeof(bm->hd));
    fo.write((char *)&bm->dib, sizeof(bm->dib));
    for (int64_t i = bm->dib.height - 1; i >= 0; --i)
    {
        for (uint32_t j = 0; j < bm->dib.width; ++j)
            fo.write((char *)&bm->bmData[i][j], sizeof(bm->bmData[i][j]));
        char temp = 0;
        for (uint32_t j = 0; j < padding; ++j)
            fo.write((char *)&temp, sizeof(temp));
    }
    fo.close();
    return true;
}

bool freeBitmap(Bitmap *&bm)
{
    if (bm != nullptr)
    {
        for (int64_t i = bm->dib.height - 1; i >= 0; --i)
        {
            delete[] bm->bmData[i];
            bm->bmData[i] = nullptr;
        }
        delete[] bm->bmData;
        bm->bmData = nullptr;
        delete bm;
        bm = nullptr;
    }
    return true;
}

bool freePixelarray(Bitmap *&bm)
{
    if (bm != nullptr)
    {
        for (uint32_t i = 0; i < bm->dib.height; ++i)
        {
            delete[] bm->bmData[i];
            bm->bmData[i] = nullptr;
        }
        delete[] bm->bmData;
        bm->bmData = nullptr;
    }
    return true;
}

Pixel **splitPixelarray(Pixel **&bm, uint32_t &width, uint32_t &height, uint32_t &w_pos, uint32_t &h_pos)
{
    Pixel **res = new Pixel *[height];
    if (res != nullptr)
    {
        for (uint32_t i = 0, ii = h_pos; i < height; ++i, ++ii)
        {
            res[i] = new Pixel[width];
            if (res[i] != nullptr)
            {
                for (uint32_t j = 0, jj = w_pos; j < width; ++j, ++jj)
                    res[i][j] = bm[ii][jj];
            }
        }
    }
    return res;
}

bool splitBitmap(Bitmap *&bm, uint32_t &h_part, uint32_t &w_part)
{
    if (bm != nullptr)
    {
        Bitmap *tiny_bm = new Bitmap;
        if (tiny_bm != nullptr)
        {
            tiny_bm->hd = bm->hd;
            tiny_bm->dib = bm->dib;
            tiny_bm->dib.height = bm->dib.height / h_part;
            tiny_bm->dib.width = bm->dib.width / w_part;
            uint32_t padding = (4 - (3 * tiny_bm->dib.width) % 4) % 4;
            tiny_bm->dib.pixelSize = tiny_bm->dib.height * ((3 * tiny_bm->dib.width) + padding);
            uint32_t fileSize = 54 + tiny_bm->dib.pixelSize, cnt = 0;
            while (cnt < 4)
            {
                tiny_bm->hd.fileSize[cnt++] = fileSize % 256;
                fileSize /= 256;
            }
            cnt = 0;
            for (uint32_t h_pos = 0; h_pos < bm->dib.height; h_pos += tiny_bm->dib.height)
                for (uint32_t w_pos = 0; w_pos < bm->dib.width; w_pos += tiny_bm->dib.width)
                {
                    tiny_bm->bmData = splitPixelarray(bm->bmData, tiny_bm->dib.width, tiny_bm->dib.height, w_pos, h_pos);
                    string filename = to_string(cnt++) + ".bmp";
                    writeBitmap(filename, tiny_bm, padding);
                    freePixelarray(tiny_bm);
                }
        }
        delete tiny_bm;
        tiny_bm = nullptr;
    }
    return (bm != nullptr);
}

bool colorizeBnW(Pixel **&bm, uint32_t &width, uint32_t &height)
{
    for (uint32_t i = 0; i < height; ++i)
        for (uint32_t j = 0; j < width; ++j)
        {
            uint32_t clr = (bm[i][j].B + bm[i][j].G + bm[i][j].R) / 3;
            bm[i][j].B = bm[i][j].G = bm[i][j].R = clr;
        }
    return true;
}

bool flipVertical(Pixel **&bm, uint32_t &width, uint32_t &height)
{
    uint32_t new_h = height / 2;
    for (uint32_t i = 0; i < new_h; ++i)
        for (uint32_t j = 0; j < width; ++j)
        {
            Pixel temp = bm[i][j];
            bm[i][j] = bm[height - 1 - i][j];
            bm[height - 1 - i][j] = temp;
        }
    return true;
}

bool flipHorizontal(Pixel **&bm, uint32_t &width, uint32_t &height)
{
    uint32_t new_w = width / 2;
    for (uint32_t i = 0; i < height; ++i)
        for (uint32_t j = 0; j < new_w; ++j)
        {
            Pixel temp = bm[i][j];
            bm[i][j] = bm[i][width - 1 - j];
            bm[i][width - 1 - j] = temp;
        }
    return true;
}

int main()
{
    short int choice = 1;
    string str = "";
    Bitmap *bm = nullptr;
    while (true)
    {
        cout << "Enter filename = ";
        cin >> str;
        bm = readBitmap(str);
        if (bm == nullptr)
            cout << "File isn't .bmp || Cannot open " << str << "." << endl;
        else
            break;
    }
    do
    {
        cout << "1. Crop image" << endl
             << "2. Grayscale image" << endl
             << "3. Flip Vertical image" << endl
             << "4. Flip Horizontal image" << endl
             << "0. Exit" << endl
             << "Your choice = ";
        cin >> choice;
        if (choice == 1)
        {
            uint32_t h = 0, w = 0;
            while (true)
            {
                cout << "Enter number of width part = ";
                cin >> w;
                cout << "Enter number of height part = ";
                cin >> h;
                if (bm->dib.height % h != 0 || bm->dib.width % w != 0)
                    cout << "Cannot crop!" << endl;
                else
                    break;
            }
            if (!splitBitmap(bm, h, w))
                cout << "Error!" << endl;
            else
                cout << "Done!" << endl;
        }
        else if (choice == 2)
        {
            uint32_t padding = (4 - (3 * bm->dib.width) % 4) % 4;
            str = "ImageAfterGrayScale.bmp";
            if (colorizeBnW(bm->bmData, bm->dib.width, bm->dib.height))
                cout << "Done!" << endl;
            writeBitmap(str, bm, padding);
        }
        else if (choice == 3)
        {
            uint32_t padding = (4 - (3 * bm->dib.width) % 4) % 4;
            str = "ImageAfterFlipVertical.bmp";
            if (flipVertical(bm->bmData, bm->dib.width, bm->dib.height))
                cout << "Done!" << endl;
            writeBitmap(str, bm, padding);
        }
        else if (choice == 4)
        {
            uint32_t padding = (4 - (3 * bm->dib.width) % 4) % 4;
            str = "ImageAfterFlipHorizontal.bmp";
            if (flipHorizontal(bm->bmData, bm->dib.width, bm->dib.height))
                cout << "Done!" << endl;
            writeBitmap(str, bm, padding);
        }
        else if (choice == 0)
            break;
    } while (choice <= 4);
    freeBitmap(bm);
    return 0;
}