#include "MovieIO.h"

#include <memory>
#include <regex>
#include <assert.h>
#include <set>

// Input Plugins
#include "OIIOMovieInput.h"
#include "OpenCVMovieInput.h"




namespace MovieIO
{
    // Factory
    std::unique_ptr<MovieInput> MovieInput::open(const std::string& name)
    {
        auto ext = std::filesystem::path(name).extension();
        if (std::set<std::filesystem::path>({ ".mp4" }).contains(ext))
        {
            return std::make_unique<OpenCVMovieInput>(name);
        }
        else if (std::set<std::filesystem::path>({ ".exr", ".jpg", ".jpeg" }).contains(ext))
        {
            return std::make_unique<OIIOMovieInput>(name);
        }
    }
 }