#include "MapReader.h"

#include "GameValues.h"
#include "map.h"
#include "MapReaderConstants.h"
#include "movingplatform.h"
#include "FileIO.h"
#include "TilesetManager.h"

#include <cassert>
#include <iostream>

extern CTilesetManager* g_tilesetmanager;


MapReader1700::MapReader1700()
    : MapReader1600()
{
    patch_version = 0;
}

MapReader1701::MapReader1701()
    : MapReader1700()
{
    patch_version = 1;
}

MapReader1702::MapReader1702()
    : MapReader1701()
{
    patch_version = 2;
}

void MapReader1702::read_autofilters(CMap& map, BinaryFile& mapfile)
{
    int32_t iAutoFilterValues[9];
    mapfile.read_i32_array(iAutoFilterValues, 9);

    for (short iFilter = 0; iFilter < 8; iFilter++)
        map.fAutoFilter[iFilter] = iAutoFilterValues[iFilter] > 0;

    for (short iFilter = 8; iFilter < NUM_AUTO_FILTERS; iFilter++)
        map.fAutoFilter[iFilter] = false;

    //Read density and don't do anything with it at the moment
    //int iDensity = iAutoFilterValues[8];
}

void MapReader1700::read_tiles(CMap& map, BinaryFile& mapfile)
{
    short iClassicTilesetID = g_tilesetmanager->indexFromName("Classic");

    unsigned short i, j, k;
    for (j = 0; j < MAPHEIGHT; j++) {
        for (i = 0; i < MAPWIDTH; i++) {
            for (k = 0; k < MAPLAYERS; k++) {
                short iTileID = (short)mapfile.read_i32();

                TilesetTile * tile = &map.mapdata[i][j][k];

                if (iTileID == TILESETSIZE) {
                    tile->iID = TILESETNONE;
                    tile->iCol = 0;
                    tile->iRow = 0;
                } else {
                    tile->iID = iClassicTilesetID;
                    tile->iCol = iTileID % TILESETWIDTH;
                    tile->iRow = iTileID / TILESETWIDTH;
                }
            }

            map.objectdata[i][j].iType = (short)mapfile.read_i32();
            if (map.objectdata[i][j].iType == 15)
                map.objectdata[i][j].iType = -1;

            map.objectdata[i][j].fHidden = false;

            if (map.objectdata[i][j].iType == 1) {
                for (short iSetting = 0; iSetting < NUM_BLOCK_SETTINGS; iSetting++)
                    map.objectdata[i][j].iSettings[iSetting] = defaultPowerupSetting(0, iSetting);
            }
        }
    }
}

void MapReader1701::read_background(CMap& map, BinaryFile& mapfile)
{
    //Read in background to use
    char text[128];
    mapfile.read_string_long(text, 128);
    map.szBackgroundFile = text;

    for (const std::string_view background : g_szBackgroundConversion) {
        // All items must have an underscore in g_szBackgroundConversion
        const size_t underscorePos = background.find("_");
        assert(underscorePos != background.npos);

        if (background.substr(underscorePos + 1) == map.szBackgroundFile) {
            map.szBackgroundFile = background;
            break;
        }
    }
}

void MapReader1702::read_background(CMap& map, BinaryFile& mapfile)
{
    //Read in background to use
    char text[128];
    mapfile.read_string_long(text, 128);
    map.szBackgroundFile = text;
}

void MapReader1700::set_preview_switches(CMap& map, BinaryFile& mapfile)
{
    // if it is a preview, for older maps, set the on/off blocks to on by default

    // FIXME
    /*if (iReadType != read_type_preview)
        return;*/

    //Read on/off switches
    for (unsigned short iSwitch = 0; iSwitch < 4; iSwitch++) {
        map.iSwitches[iSwitch] = 1;
    }

    //Set all the on/off blocks correctly
    for (unsigned short j = 0; j < MAPHEIGHT; j++) {
        for (unsigned short i = 0; i < MAPWIDTH; i++) {
            if (map.objectdata[i][j].iType >= 11 && map.objectdata[i][j].iType <= 14) {
                map.objectdata[i][j].iSettings[0] = 1;
            }
        }
    }
}

void MapReader1700::read_switches(CMap& map, BinaryFile& mapfile)
{
    //Read on/off switches
    for (unsigned short iSwitch = 0; iSwitch < 4; iSwitch++) {
        map.iSwitches[iSwitch] = 1 - (short)mapfile.read_i32();
    }

    //Set all the on/off blocks correctly
    for (unsigned short j = 0; j < MAPHEIGHT; j++) {
        for (unsigned short i = 0; i < MAPWIDTH; i++) {
            if (map.objectdata[i][j].iType >= 11 && map.objectdata[i][j].iType <= 14) {
                map.objectdata[i][j].iSettings[0] = map.iSwitches[map.objectdata[i][j].iType - 11];
            }
        }
    }
}

void MapReader1701::read_music_category(CMap& map, BinaryFile& mapfile)
{
    map.musicCategoryID = mapfile.read_i32();
}

void MapReader1700::read_warp_locations(CMap& map, BinaryFile& mapfile)
{
    for (unsigned short j = 0; j < MAPHEIGHT; j++) {
        for (unsigned short i = 0; i < MAPWIDTH; i++) {
            map.mapdatatop[i][j] = (TileType)mapfile.read_i32();

            map.warpdata[i][j].direction = (WarpEnterDirection)mapfile.read_i32();
            map.warpdata[i][j].connection = (short)mapfile.read_i32();
            map.warpdata[i][j].id = (short)mapfile.read_i32();

            for (short sType = 0; sType < 6; sType += 5)
                map.nospawn[sType][i][j] = mapfile.read_i32() == 0 ? false : true;

            //Copy player no spawn areas into team no spawn areas
            for (short sType = 1; sType < 5; sType++)
                map.nospawn[sType][i][j] = map.nospawn[0][i][j];

        }
    }
}

bool MapReader1700::read_spawn_areas(CMap& map, BinaryFile& mapfile)
{
    //Read spawn areas
    for (unsigned short i = 0; i < 6; i += 5) {
        map.totalspawnsize[i] = 0;
        map.numspawnareas[i] = (short)mapfile.read_i32();

        if (map.numspawnareas[i] > MAXSPAWNAREAS) {
            std::cout << std::endl << " ERROR: Number of spawn areas (" << map.numspawnareas[i]
                << ") was greater than max allowed (" << MAXSPAWNAREAS << ')'
                << std::endl;
            return false;
        }

        for (int m = 0; m < map.numspawnareas[i]; m++) {
            map.spawnareas[i][m].left = (short)mapfile.read_i32();
            map.spawnareas[i][m].top = (short)mapfile.read_i32();
            map.spawnareas[i][m].width = (short)mapfile.read_i32();
            map.spawnareas[i][m].height = (short)mapfile.read_i32();
            map.spawnareas[i][m].size = (short)mapfile.read_i32();

            map.totalspawnsize[i] += map.spawnareas[i][m].size;
        }
    }

    //Copy player spawn areas to team specific spawn areas
    for (short iType = 1; iType < 5; iType++) {
        map.totalspawnsize[iType] = map.totalspawnsize[0];
        map.numspawnareas[iType] = map.numspawnareas[0];

        for (int m = 0; m < map.numspawnareas[0]; m++) {
            map.spawnareas[iType][m].left = map.spawnareas[0][m].left;
            map.spawnareas[iType][m].top = map.spawnareas[0][m].top;
            map.spawnareas[iType][m].width = map.spawnareas[0][m].width;
            map.spawnareas[iType][m].height = map.spawnareas[0][m].height;
            map.spawnareas[iType][m].size = map.spawnareas[0][m].size;
        }
    }

    return true;
}

void MapReader1700::read_platforms(CMap& map, BinaryFile& mapfile, bool fPreview)
{
    map.clearPlatforms();

    //Load moving platforms
    const size_t iNumPlatforms = (short)mapfile.read_i32();
    map.platforms.reserve(iNumPlatforms);

    for (size_t idx = 0; idx < iNumPlatforms; idx++) {
        short iWidth = (short)mapfile.read_i32();
        short iHeight = (short)mapfile.read_i32();

        auto [tiles, types] = read_platform_tiles(map, mapfile, iWidth, iHeight);

        short iDrawLayer = 2;
        //printf("Layer: %d\n", iDrawLayer);

        short iPathType = 0;
        //printf("PathType: %d\n", iPathType);

        MovingPlatformPath* path = read_platform_path_details(mapfile, iPathType, fPreview);
        if (!path)
            continue;

        MovingPlatform* platform = new MovingPlatform(std::move(tiles), std::move(types), iWidth, iHeight, iDrawLayer, path, fPreview);
        map.platforms.emplace_back(platform);
        map.platformdrawlayer[iDrawLayer].push_back(platform);
    }
}

std::pair<std::vector<TilesetTile>, std::vector<TileType>>
MapReader1700::read_platform_tiles(CMap& map, BinaryFile& mapfile, short iWidth, short iHeight)
{
    std::vector<TilesetTile> tiles;
    std::vector<TileType> types;
    tiles.reserve(iWidth * iHeight);
    types.reserve(iWidth * iHeight);

    for (short iCol = 0; iCol < iWidth; iCol++) {
        for (short iRow = 0; iRow < iHeight; iRow++) {
            short iTile = mapfile.read_i32();

            TilesetTile tile;
            TileType type;

            if (iTile == TILESETSIZE) {
                tile.iID = TILESETNONE;
                tile.iCol = 0;
                tile.iRow = 0;

                type = TileType::NonSolid;
            } else {
                tile.iID = g_tilesetmanager->classicTilesetIndex();
                tile.iCol = iTile % TILESETWIDTH;
                tile.iRow = iTile / TILESETWIDTH;

                type = g_tilesetmanager->classicTileset().tileType(tile.iCol, tile.iRow);
            }

            tiles.emplace_back(std::move(tile));
            types.emplace_back(std::move(type));
        }
    }

    return {tiles, types};
}

MovingPlatformPath* MapReader1700::read_platform_path_details(BinaryFile& mapfile, short iPathType, bool fPreview)
{
    if (iPathType == 0) { //segment path
        float startX = mapfile.read_float();
        float startY = mapfile.read_float();
        float endX = mapfile.read_float();
        float endY = mapfile.read_float();
        float speed = mapfile.read_float();

        return new StraightPath(speed, Vec2f(startX, startY), Vec2f(endX, endY), fPreview);

        //printf("Read segment path\n");
        //printf("StartX: %.2f StartY:%.2f EndX:%.2f EndY:%.2f Velocity:%.2f\n", startX, startY, endX, endY, speed);
    }
    if (iPathType == 1) { //continuous path
        float startX = mapfile.read_float();
        float startY = mapfile.read_float();
        float angle = mapfile.read_float();
        float speed = mapfile.read_float();

        return new StraightPathContinuous(speed, Vec2f(startX, startY), angle, fPreview);

        //printf("Read continuous path\n");
        //printf("StartX: %.2f StartY:%.2f Angle:%.2f Velocity:%.2f\n", startX, startY, angle, speed);
    }
    if (iPathType == 2) { //elliptical path
        float radiusX = mapfile.read_float();
        float radiusY = mapfile.read_float();
        float centerX = mapfile.read_float();
        float centerY = mapfile.read_float();
        float angle = mapfile.read_float();
        float speed = mapfile.read_float();

        return new EllipsePath(speed, angle, Vec2f(radiusX, radiusY), Vec2f(centerX, centerY), fPreview);

        //printf("Read elliptical path\n");
        //printf("CenterX: %.2f CenterY:%.2f Angle:%.2f RadiusX: %.2f RadiusY: %.2f Velocity:%.2f\n", centerX, centerY, angle, radiusX, radiusY, speed);
    }
    return nullptr;
}

bool MapReader1700::load(CMap& map, BinaryFile& mapfile, ReadType readtype)
{
    read_autofilters(map, mapfile);

    if (readtype == read_type_summary)
        return true;

    map.clearPlatforms();

    read_tiles(map, mapfile);
    read_background(map, mapfile);

    if (patch_version >= 1)
        read_switches(map, mapfile);
    else if (readtype != read_type_preview)
        set_preview_switches(map, mapfile);

    if (patch_version >= 2) {
        //short translationid[1] = {g_tilesetmanager->GetIndexFromName("Classic")};
        read_platforms(map, mapfile, readtype == read_type_preview);
    }

    read_eyecandy(map, mapfile);
    read_music_category(map, mapfile);
    read_warp_locations(map, mapfile);

    if (readtype == read_type_preview)
        return true;

    read_warp_exits(map, mapfile);
    read_spawn_areas(map, mapfile);
    if (!read_draw_areas(map, mapfile))
        return false;

    if (patch_version <= 1) {
        //short translationid[1] = {g_tilesetmanager->GetIndexFromName("Classic")};
        read_platforms(map, mapfile, readtype == read_type_preview);
    }

    if (patch_version == 0)
        read_switches(map, mapfile);

    return true;
}
