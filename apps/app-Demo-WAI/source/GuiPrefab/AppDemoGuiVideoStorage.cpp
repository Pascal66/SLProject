//#############################################################################
//  File:      AppDemoGuiVideoStorage.cpp
//  Author:    Luc Girod
//  Date:      April 2019
//  Codestyle: https://github.com/cpvrlab/SLProject/wiki/Coding-Style-Guidelines
//  Copyright: Marcus Hudritsch
//             This software is provide under the GNU General Public License
//             Please visit: http://opensource.org/licenses/GPL-3.0
//#############################################################################

#include <imgui.h>
#include <imgui_internal.h>
#include <stdio.h>

#include <Utils.h>
#include <AppDemoGuiVideoStorage.h>
#include <SLApplication.h>
#include <CVCapture.h>

//-----------------------------------------------------------------------------

AppDemoGuiVideoStorage::AppDemoGuiVideoStorage(const std::string& name, std::string videoDir, cv::VideoWriter* videoWriter, cv::VideoWriter* videoWriterInfo, std::ofstream* gpsDataStream, bool* activator)
  : AppDemoGuiInfosDialog(name, activator),
    _videoPrefix("video-"),
    _nextId(0),
    _videoWriter(videoWriter),
    _videoWriterInfo(videoWriterInfo),
    _gpsDataFile(gpsDataStream)
{
    _videoDir    = Utils::unifySlashes(videoDir);
    _currentItem = "";

    _existingVideoNames.clear();
    vector<pair<int, string>> existingVideoNamesSorted;

    //check if visual odometry maps directory exists
    if (!Utils::dirExists(_videoDir))
    {
        Utils::makeDir(_videoDir);
    }
    else
    {
        //parse content: we search for directories in mapsDir
        std::vector<std::string> content = Utils::getFileNamesInDir(_videoDir);
        for (auto path : content)
        {
            std::string name = Utils::getFileName(path);
            //find json files that contain mapPrefix and estimate highest used id
            if (Utils::containsString(name, _videoPrefix))
            {
                //estimate highest used id
                std::vector<std::string> splitted;
                Utils::splitString(name, '-', splitted);
                if (splitted.size())
                {
                    int id = atoi(splitted.back().c_str());
                    existingVideoNamesSorted.push_back(make_pair(id, name));
                    if (id >= _nextId)
                    {
                        _nextId = id + 1;
                    }
                }
            }
        }
    }

    //sort existingMapNames
    std::sort(existingVideoNamesSorted.begin(), existingVideoNamesSorted.end(), [](const pair<int, string>& left, const pair<int, string>& right) { return left.first < right.first; });
    for (auto it = existingVideoNamesSorted.begin(); it != existingVideoNamesSorted.end(); ++it)
        _existingVideoNames.push_back(it->second);
}
//-----------------------------------------------------------------------------

void AppDemoGuiVideoStorage::saveVideo(std::string filename)
{
    std::string infoDir  = _videoDir + "info/";
    std::string infoPath = infoDir + filename;
    std::string path     = _videoDir + filename;

    if (!Utils::dirExists(_videoDir))
    {
        Utils::makeDir(_videoDir);
    }
    else
    {
        if (Utils::fileExists(path))
        {
            Utils::deleteFile(path);
        }
    }

    if (!Utils::dirExists(infoDir))
    {
        Utils::makeDir(infoDir);
    }
    else
    {
        if (Utils::fileExists(infoPath))
        {
            Utils::deleteFile(infoPath);
        }
    }

    if (_videoWriter->isOpened())
    {
        _videoWriter->release();
    }
    if (_videoWriterInfo->isOpened())
    {
        _videoWriterInfo->release();
    }

    cv::Size size = cv::Size(CVCapture::instance()->lastFrame.cols, CVCapture::instance()->lastFrame.rows);

    bool ret = _videoWriter->open(path, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, size, true);

    ret = _videoWriterInfo->open(infoPath, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, size, true);
}

void AppDemoGuiVideoStorage::saveGPSData(std::string videofile)
{
    std::string filename = Utils::getFileNameWOExt(videofile) + ".txt";
    std::string path     = _videoDir + filename;
    _gpsDataFile->open(filename);
}

//-----------------------------------------------------------------------------
void AppDemoGuiVideoStorage::buildInfos(SLScene* s, SLSceneView* sv)
{
    ImGui::Begin("Video storage", _activator, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Button("Start recording", ImVec2(ImGui::GetContentRegionAvailWidth(), 0.0f)))
    {
        std::string filename = Utils::getDateTime2String() + "_" + SLApplication::getComputerInfos() + ".avi";
        saveVideo(filename);
    }

    ImGui::Separator();

    if (ImGui::Button("Stop recording", ImVec2(ImGui::GetContentRegionAvailWidth(), 0.0f)))
    {
        _videoWriter->release();
        _gpsDataFile->close();
    }

    ImGui::Separator();
    if (ImGui::Button("New video", ImVec2(ImGui::GetContentRegionAvailWidth(), 0.0f)))
    {
        _currentItem = "";
        _nextId      = _nextId + 1;
        _videoWriter->release();
    }

    ImGui::Separator();
    {
        if (ImGui::BeginCombo("Current", _currentItem.c_str())) // The second parameter is the label previewed before opening the combo.
        {
            for (int n = 0; n < _existingVideoNames.size(); n++)
            {
                bool isSelected = (_currentItem == _existingVideoNames[n]); // You can store your selection however you want, outside or inside your objects
                if (ImGui::Selectable(_existingVideoNames[n].c_str(), isSelected))
                {
                    _currentItem = _existingVideoNames[n];
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus(); // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();
}
