/*========================================================================
  VES --- VTK OpenGL ES Rendering Toolkit

      http://www.kitware.com/ves

  Copyright 2011 Kitware, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 ========================================================================*/

#include "vesKiwiViewerApp.h"
#include "vesKiwiCurlDownloader.h"
#include "vesKiwiDataConversionTools.h"
#include "vesKiwiDataLoader.h"
#include "vesKiwiDataRepresentation.h"
#include "vesKiwiImagePlaneDataRepresentation.h"
#include "vesKiwiImageWidgetRepresentation.h"
#include "vesKiwiAnimationRepresentation.h"
#include "vesKiwiBrainAtlasRepresentation.h"
#include "vesKiwiText2DRepresentation.h"
#include "vesKiwiPlaneWidget.h"
#include "vesKiwiPolyDataRepresentation.h"

#include "vesCamera.h"
#include "vesColorUniform.h"
#include "vesMath.h"
#include "vesModelViewUniform.h"
#include "vesNormalMatrixUniform.h"
#include "vesProjectionUniform.h"
#include "vesRenderer.h"
#include "vesActor.h"
#include "vesShader.h"
#include "vesShaderProgram.h"
#include "vesTexture.h"
#include "vesUniform.h"
#include "vesVertexAttribute.h"
#include "vesVertexAttributeKeys.h"
#include "vesBuiltinShaders.h"

#include "vesPVWebClient.h"
#include "vesPVWebDataSet.h"

#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDelimitedTextReader.h>
#include <vtkTable.h>


#include <vtksys/SystemTools.hxx>

#include <cassert>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>


//----------------------------------------------------------------------------
class vesKiwiViewerApp::vesInternal
{
public:

  vesInternal()
  {
    this->IsAnimating = false;
  }

  ~vesInternal()
  {
    this->DataRepresentations.clear();
    this->BuiltinDatasetNames.clear();
    this->BuiltinDatasetFilenames.clear();
    this->BuiltinShadingModels.clear();
  }

  struct vesShaderProgramData
  {
    vesShaderProgramData(
      const std::string &name, vesSharedPtr<vesShaderProgram> shaderProgram) :
      Name(name), ShaderProgram(shaderProgram)
    {
    }

    std::string Name;
    vesSharedPtr<vesShaderProgram> ShaderProgram;
  };

  struct vesCameraParameters
  {
    vesCameraParameters()
    {
      this->setParameters(vesVector3f(0.0, 0.0, -1.0), vesVector3f(0.0, 1.0, 0.0));
    }

    void setParameters(const vesVector3f& viewDirection, const vesVector3f& viewUp)
    {
      this->ViewDirection = viewDirection;
      this->ViewUp = viewUp;
    }

    vesVector3f ViewDirection;
    vesVector3f ViewUp;
  };

  bool setShaderProgramOnRepresentations(
    vesSharedPtr<vesShaderProgram> shaderProgram);

  bool IsAnimating;
  std::string ErrorTitle;
  std::string ErrorMessage;

  vesSharedPtr<vesShaderProgram> ShaderProgram;
  vesSharedPtr<vesShaderProgram> TextureShader;
  vesSharedPtr<vesShaderProgram> GouraudTextureShader;
  vesSharedPtr<vesShaderProgram> ClipShader;
  vesSharedPtr<vesUniform> ClipUniform;

  std::vector<vesKiwiDataRepresentation*> DataRepresentations;

  vesKiwiDataLoader DataLoader;

  std::vector<std::string> BuiltinDatasetNames;
  std::vector<std::string> BuiltinDatasetFilenames;
  std::vector<vesCameraParameters> BuiltinDatasetCameraParameters;

  std::string CurrentShadingModel;
  std::vector<vesShaderProgramData> BuiltinShadingModels;
};

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::vesInternal::setShaderProgramOnRepresentations(
  vesSharedPtr<vesShaderProgram> shaderProgram)
{
  bool success = false;

  for (size_t i = 0; i < this->DataRepresentations.size(); ++i) {
    vesKiwiPolyDataRepresentation *polyDataRepresentation =
        dynamic_cast<vesKiwiPolyDataRepresentation*>(this->DataRepresentations[i]);

    vesKiwiImageWidgetRepresentation *imageWidgetRepresentation =
        dynamic_cast<vesKiwiImageWidgetRepresentation*>(this->DataRepresentations[i]);

    if (polyDataRepresentation) {
      polyDataRepresentation->setShaderProgram(shaderProgram);
      success = true;
    }
    else if (imageWidgetRepresentation) {
      imageWidgetRepresentation->setShaderProgram(shaderProgram);
      success = true;
    }
  }

  return success;
}

//----------------------------------------------------------------------------
vesKiwiViewerApp::vesKiwiViewerApp()
{
  this->Internal = new vesInternal();
  this->resetScene();

  this->addBuiltinDataset("Utah Teapot", "teapot.vtp");
  this->addBuiltinDataset("Stanford Bunny", "bunny.vtp");
  this->addBuiltinDataset("NLM Visible Woman Hand", "visible-woman-hand.vtp");
  this->addBuiltinDataset("NA-MIC Knee Atlas", "AppendedKneeData.vtp");

  this->addBuiltinDataset("ROS C Turtle", "cturtle.vtp");
  this->Internal->BuiltinDatasetCameraParameters.back().setParameters(
    vesVector3f(0.,0.,1.), vesVector3f(0.,-1.,0.));

  this->addBuiltinDataset("Mount St. Helens", "MountStHelen.vtp");
  this->addBuiltinDataset("Space Shuttle", "shuttle.vtp");

  //http://visibleearth.nasa.gov/view.php?id=57730
  this->addBuiltinDataset("NASA Blue Marble", "textured_sphere.vtp");
  this->Internal->BuiltinDatasetCameraParameters.back().setParameters(
    vesVector3f(1.,0.,0.), vesVector3f(0.,0.,1.));

  this->addBuiltinDataset("Buckyball", "Buckyball.vtp");
  this->addBuiltinDataset("Caffeine", "caffeine.pdb");

  this->addBuiltinDataset("Head CT Image", "head.vti");
  this->Internal->BuiltinDatasetCameraParameters.back().setParameters(
    vesVector3f(1.,0.,0.), vesVector3f(0.,0.,-1.));

  this->addBuiltinDataset("KiwiViewer Logo", "kiwi.png");

  this->addBuiltinDataset("SPL-PNL Brain Atlas", "model_info.txt");
  this->Internal->BuiltinDatasetCameraParameters.back().setParameters(
    vesVector3f(-1.,0.,0.), vesVector3f(0.,0.,1.));

  this->addBuiltinDataset("Can Simulation", "can0000.vtp");
  this->Internal->BuiltinDatasetCameraParameters.back().setParameters(
    vesVector3f(0.,1.,0.), vesVector3f(0.,0.,1.));

  this->addBuiltinDataset("ParaView Web", "pvweb");

  this->initBlinnPhongShader(
    vesBuiltinShaders::vesBlinnPhong_vert(),
    vesBuiltinShaders::vesBlinnPhong_frag());
  this->initToonShader(
    vesBuiltinShaders::vesToonShader_vert(),
    vesBuiltinShaders::vesToonShader_frag());
  this->initGouraudShader(
    vesBuiltinShaders::vesShader_vert(),
    vesBuiltinShaders::vesShader_frag());
  this->initGouraudTextureShader(
    vesBuiltinShaders::vesGouraudTexture_vert(),
    vesBuiltinShaders::vesGouraudTexture_frag());
  this->initTextureShader(
    vesBuiltinShaders::vesBackgroundTexture_vert(),
    vesBuiltinShaders::vesBackgroundTexture_frag());
  this->initClipShader(
    vesBuiltinShaders::vesClipPlane_vert(),
    vesBuiltinShaders::vesClipPlane_frag());

  this->setShadingModel("Gouraud");
}

//----------------------------------------------------------------------------
vesKiwiViewerApp::~vesKiwiViewerApp()
{
  this->removeAllDataRepresentations();
  delete this->Internal;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::checkForPVWebError(vesPVWebClient::Ptr client)
{
  this->setErrorMessage(client->errorTitle(), client->errorMessage());
  return !client->errorMessage().empty();
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::doPVWebTest(const std::string& host, const std::string& sessionId)
{
  this->resetScene();

  vesPVWebClient::Ptr client(new vesPVWebClient);
  client->setHost(host);

  if (sessionId.empty()) {

    if (!client->createVisualization()) {
      this->checkForPVWebError(client);
      return false;
    }

    client->configureOff();
    
    if (client->createView()) {
      client->executeCommand("Sphere");
      client->executeCommand("Show");
      client->executeCommand("Render");
      client->executeCommand("ResetCamera");
      client->configureOn();
      client->executeCommand("Render");
      client->pollSceneMetaData();
      client->downloadObjects();
    }

    client->endVisualization();

  }
  else {

    client->setSessionId(sessionId);

    if (client->createView()) {
      client->configureOn();
      client->executeCommand("Render");
      client->executeCommand("GetProxy");
      client->pollSceneMetaData();
      client->downloadObjects();
    }
  }

  if (this->checkForPVWebError(client)) {
    return false;
  }

  for (size_t i = 0; i < client->datasets().size(); ++i) {

    const vesPVWebDataSet::Ptr dataset = client->datasets()[i];

    if (dataset->m_datasetType == 'P')
      continue;

    if (dataset->m_layer != 0)
      continue;

    if (dataset->m_numberOfVerts == 0)
      continue;

    vesKiwiPolyDataRepresentation* rep = new vesKiwiPolyDataRepresentation();
    rep->initializeWithShader(this->shaderProgram());
    rep->setPVWebData(dataset);
    rep->addSelfToRenderer(this->renderer());
    this->Internal->DataRepresentations.push_back(rep);
  }

  this->resetView();

  return true;
}

//----------------------------------------------------------------------------
std::string vesKiwiViewerApp::downloadFile(const std::string& url, const std::string& downloadDir)
{
  vesKiwiCurlDownloader downloader;
  const std::string destFile = downloadDir + "/kiwiviewer_download.temp";

  if (!downloader.downloadFile(url, destFile)) {
    this->setErrorMessage(downloader.errorTitle(), downloader.errorMessage());
    return std::string();
  }

  std::string newName = downloader.attachmentFileName();
  if (newName.empty()) {
    newName = vtksys::SystemTools::GetFilenameName(url);
  }

  if (newName.empty()) {
    this->setErrorMessage("Download Error", "Could not determine filename from URL.");
    return std::string();
  }

  newName = downloadDir + "/" + newName;

  if (!this->renameFile(destFile, newName)) {
    return std::string();
  }

  return newName;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::renameFile(const std::string& srcFile, const std::string& destFile)
{
  std::string destDir = vtksys::SystemTools::GetFilenamePath(destFile);
  if (!vtksys::SystemTools::MakeDirectory(destDir.c_str())) {
    this->setErrorMessage("File Error", "Failed to create directory: " + destDir);
    return false;
  }

  if (rename(srcFile.c_str(), destFile.c_str()) != 0) {
    this->setErrorMessage("File Error", "Failed to rename file: " + srcFile);
    return false;
  }
}

//----------------------------------------------------------------------------
const vesSharedPtr<vesShaderProgram> vesKiwiViewerApp::shaderProgram() const
{
  return this->Internal->ShaderProgram;
}

//----------------------------------------------------------------------------
vesSharedPtr<vesShaderProgram> vesKiwiViewerApp::shaderProgram()
{
  return this->Internal->ShaderProgram;
}

//----------------------------------------------------------------------------
int vesKiwiViewerApp::numberOfBuiltinDatasets() const
{
  return static_cast<int>(this->Internal->BuiltinDatasetNames.size());
}

//----------------------------------------------------------------------------
int vesKiwiViewerApp::defaultBuiltinDatasetIndex() const
{
  return 6;
}

//----------------------------------------------------------------------------
std::string vesKiwiViewerApp::builtinDatasetName(int index)
{
  assert(index >= 0 && index < this->numberOfBuiltinDatasets());
  return this->Internal->BuiltinDatasetNames[index];
}

//----------------------------------------------------------------------------
std::string vesKiwiViewerApp::builtinDatasetFilename(int index)
{
  assert(index >= 0 && index < this->numberOfBuiltinDatasets());
  return this->Internal->BuiltinDatasetFilenames[index];
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::addBuiltinDataset(const std::string& name, const std::string& filename)
{
  this->Internal->BuiltinDatasetNames.push_back(name);
  this->Internal->BuiltinDatasetFilenames.push_back(filename);
  this->Internal->BuiltinDatasetCameraParameters.push_back(vesInternal::vesCameraParameters());
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::applyBuiltinDatasetCameraParameters(int index)
{
  this->resetView(this->Internal->BuiltinDatasetCameraParameters[index].ViewDirection,
                  this->Internal->BuiltinDatasetCameraParameters[index].ViewUp);
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::addBuiltinShadingModel(
  const std::string &name, vesSharedPtr<vesShaderProgram> shaderProgram)
{
  assert(shaderProgram);

  if (!shaderProgram) {
    return;
  }

  for(size_t i=0; i < this->Internal->BuiltinShadingModels.size(); ++i) {
    if (this->Internal->BuiltinShadingModels[i].Name == name) {
      this->deleteShaderProgram(this->Internal->BuiltinShadingModels[i].ShaderProgram);
      this->Internal->BuiltinShadingModels[i].ShaderProgram = shaderProgram;
      return;
    }
  }

  this->Internal->BuiltinShadingModels.push_back(
    vesInternal::vesShaderProgramData(name, shaderProgram));
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::isAnimating() const
{
  return this->Internal->IsAnimating;
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::setAnimating(bool animating)
{
  this->Internal->IsAnimating = animating;
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::willRender()
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    this->Internal->DataRepresentations[i]->willRender(this->renderer());
  }
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::handleSingleTouchPanGesture(double deltaX, double deltaY)
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    vesKiwiWidgetRepresentation* rep = dynamic_cast<vesKiwiWidgetRepresentation*>(this->Internal->DataRepresentations[i]);
    if (rep) {
      if (rep->handleSingleTouchPanGesture(deltaX, deltaY)) {
        return;
      }
    }
  }

  this->Superclass::handleSingleTouchPanGesture(deltaX, deltaY);
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::handleDoubleTap(int displayX, int displayY)
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    vesKiwiWidgetRepresentation* rep = dynamic_cast<vesKiwiWidgetRepresentation*>(this->Internal->DataRepresentations[i]);
    if (rep) {
      if (rep->handleDoubleTap(displayX, displayY)) {
        return;
      }
    }
  }
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::handleLongPress(int displayX, int displayY)
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    vesKiwiWidgetRepresentation* rep = dynamic_cast<vesKiwiWidgetRepresentation*>(this->Internal->DataRepresentations[i]);
    if (rep) {
      if (rep->handleLongPress(displayX, displayY)) {
        return;
      }
    }
  }
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::handleSingleTouchUp()
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    vesKiwiWidgetRepresentation* rep = dynamic_cast<vesKiwiWidgetRepresentation*>(this->Internal->DataRepresentations[i]);
    if (rep) {
      if (rep->handleSingleTouchUp()) {
        return;
      }
    }
  }
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::handleSingleTouchTap(int displayX, int displayY)
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    vesKiwiWidgetRepresentation* rep = dynamic_cast<vesKiwiWidgetRepresentation*>(this->Internal->DataRepresentations[i]);
    if (rep) {
      if (rep->handleSingleTouchTap(displayX, displayY)) {
        return;
      }
    }
  }
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::handleSingleTouchDown(int displayX, int displayY)
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    vesKiwiWidgetRepresentation* rep = dynamic_cast<vesKiwiWidgetRepresentation*>(this->Internal->DataRepresentations[i]);
    if (rep) {
      if (rep->handleSingleTouchDown(displayX, displayY)) {
        return;
      }
    }
  }
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::widgetInteractionIsActive() const
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    vesKiwiWidgetRepresentation* rep = dynamic_cast<vesKiwiWidgetRepresentation*>(this->Internal->DataRepresentations[i]);
    if (rep) {
      if (rep->interactionIsActive()) {
        return true;
      }
    }
  }
  return false;
}

//----------------------------------------------------------------------------
int vesKiwiViewerApp::getNumberOfShadingModels() const
{
  return static_cast<int>(this->Internal->BuiltinShadingModels.size());
}

//----------------------------------------------------------------------------
std::string vesKiwiViewerApp::getCurrentShadingModel() const
{
  return this->Internal->CurrentShadingModel;
}

//----------------------------------------------------------------------------
std::string vesKiwiViewerApp::getShadingModel(int index) const
{
  std::string empty;
  if(index < 0 ||
     index > static_cast<int>(this->Internal->BuiltinShadingModels.size()))
  {
  return empty;
  }

  return this->Internal->BuiltinShadingModels[index].Name;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::setShadingModel(const std::string& name)
{
  bool success = true;

  for(size_t i=0; i < this->Internal->BuiltinShadingModels.size(); ++i) {
    // Check if we have a match.
    if (this->Internal->BuiltinShadingModels[i].Name == name) {
      // Check if we have a valid shader program before we switch.
      if (this->Internal->BuiltinShadingModels[i].ShaderProgram) {
        this->Internal->ShaderProgram =
          this->Internal->BuiltinShadingModels[i].ShaderProgram;

        return this->Internal->setShaderProgramOnRepresentations(
          this->Internal->ShaderProgram);
      }
    }
  }

  return !success;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::initGouraudShader(const std::string& vertexSource, const std::string& fragmentSource)
{
  vesSharedPtr<vesShaderProgram> shaderProgram
    = this->addShaderProgram(vertexSource, fragmentSource);
  this->addModelViewMatrixUniform(shaderProgram);
  this->addProjectionMatrixUniform(shaderProgram);
  this->addNormalMatrixUniform(shaderProgram);
  this->addVertexPositionAttribute(shaderProgram);
  this->addVertexNormalAttribute(shaderProgram);
  this->addVertexColorAttribute(shaderProgram);
  this->Internal->ShaderProgram = shaderProgram;

  this->addBuiltinShadingModel("Gouraud", shaderProgram);

  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::initBlinnPhongShader(const std::string& vertexSource,
                                            const std::string& fragmentSource)
{
  vesSharedPtr<vesShaderProgram> shaderProgram
    = this->addShaderProgram(vertexSource, fragmentSource);
  this->addModelViewMatrixUniform(shaderProgram);
  this->addProjectionMatrixUniform(shaderProgram);
  this->addNormalMatrixUniform(shaderProgram);
  this->addVertexPositionAttribute(shaderProgram);
  this->addVertexNormalAttribute(shaderProgram);
  this->addVertexColorAttribute(shaderProgram);
  this->Internal->ShaderProgram = shaderProgram;

  this->addBuiltinShadingModel("BlinnPhong", shaderProgram);

  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::initToonShader(const std::string& vertexSource,
                                      const std::string& fragmentSource)
{
  vesSharedPtr<vesShaderProgram> shaderProgram
    = this->addShaderProgram(vertexSource, fragmentSource);
  this->addModelViewMatrixUniform(shaderProgram);
  this->addProjectionMatrixUniform(shaderProgram);
  this->addNormalMatrixUniform(shaderProgram);
  this->addVertexPositionAttribute(shaderProgram);
  this->addVertexNormalAttribute(shaderProgram);

  this->Internal->ShaderProgram = shaderProgram;

  this->addBuiltinShadingModel("Toon", shaderProgram);

  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::initTextureShader(const std::string& vertexSource,
                                         const std::string& fragmentSource)
{
  vesSharedPtr<vesShaderProgram> shaderProgram
    = this->addShaderProgram(vertexSource, fragmentSource);
  this->addModelViewMatrixUniform(shaderProgram);
  this->addProjectionMatrixUniform(shaderProgram);
  this->addVertexPositionAttribute(shaderProgram);
  this->addVertexTextureCoordinateAttribute(shaderProgram);
  this->Internal->TextureShader = shaderProgram;
  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::initGouraudTextureShader(const std::string& vertexSource, const std::string& fragmentSource)
{
  vesShaderProgram::Ptr shaderProgram = this->addShaderProgram(vertexSource, fragmentSource);
  this->addModelViewMatrixUniform(shaderProgram);
  this->addProjectionMatrixUniform(shaderProgram);
  this->addNormalMatrixUniform(shaderProgram);
  this->addVertexPositionAttribute(shaderProgram);
  this->addVertexNormalAttribute(shaderProgram);
  this->addVertexTextureCoordinateAttribute(shaderProgram);
  this->Internal->GouraudTextureShader = shaderProgram;
  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::initClipShader(const std::string& vertexSource, const std::string& fragmentSource)
{
  vesShaderProgram::Ptr shaderProgram = this->addShaderProgram(vertexSource, fragmentSource);
  this->addModelViewMatrixUniform(shaderProgram);
  this->addProjectionMatrixUniform(shaderProgram);
  this->addNormalMatrixUniform(shaderProgram);
  this->addVertexPositionAttribute(shaderProgram);
  this->addVertexNormalAttribute(shaderProgram);
  this->addVertexColorAttribute(shaderProgram);
  this->addVertexTextureCoordinateAttribute(shaderProgram);
  this->Internal->ClipShader = shaderProgram;

  this->Internal->ClipUniform = vesUniform::Ptr(new vesUniform("clipPlaneEquation", vesVector4f(1.0f, 0.0f, 0.0f, 0.0f)));
  this->Internal->ClipShader->addUniform(this->Internal->ClipUniform);
  return true;
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::resetScene()
{
  this->resetErrorMessage();
  this->removeAllDataRepresentations();
  this->setDefaultBackgroundColor();
  this->setAnimating(false);
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::removeAllDataRepresentations()
{
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    vesKiwiDataRepresentation* rep = this->Internal->DataRepresentations[i];
    rep->removeSelfFromRenderer(this->renderer());
    delete rep;
  }
  this->Internal->DataRepresentations.clear();
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::addRepresentationsForDataSet(vtkDataSet* dataSet)
{

  if (vtkPolyData::SafeDownCast(dataSet)) {
    this->addPolyDataRepresentation(vtkPolyData::SafeDownCast(dataSet), this->shaderProgram());
  }
  else if (vtkImageData::SafeDownCast(dataSet)) {

    vtkImageData* image = vtkImageData::SafeDownCast(dataSet);

    if (image->GetDataDimension() == 3) {

      vesKiwiImageWidgetRepresentation* rep = new vesKiwiImageWidgetRepresentation();
      rep->initializeWithShader(this->shaderProgram(), this->Internal->TextureShader);
      rep->setImageData(image);
      rep->addSelfToRenderer(this->renderer());
      this->Internal->DataRepresentations.push_back(rep);

    }
    else {

      vesKiwiImagePlaneDataRepresentation* rep = new vesKiwiImagePlaneDataRepresentation();
      rep->initializeWithShader(this->Internal->TextureShader);
      rep->setImageData(image);
      rep->addSelfToRenderer(this->renderer());
      this->Internal->DataRepresentations.push_back(rep);

    }
  }
}

//----------------------------------------------------------------------------
vesKiwiPolyDataRepresentation* vesKiwiViewerApp::addPolyDataRepresentation(
  vtkPolyData* polyData, vesSharedPtr<vesShaderProgram> program)
{
  vesKiwiPolyDataRepresentation* rep = new vesKiwiPolyDataRepresentation();
  rep->initializeWithShader(program);
  rep->setPolyData(polyData);
  rep->addSelfToRenderer(this->renderer());
  this->Internal->DataRepresentations.push_back(rep);
  return rep;
}

//----------------------------------------------------------------------------
vesKiwiText2DRepresentation* vesKiwiViewerApp::addTextRepresentation(const std::string& text)
{
  vesKiwiText2DRepresentation* rep = new vesKiwiText2DRepresentation();
  rep->initializeWithShader(this->Internal->TextureShader);
  rep->setText(text);
  rep->addSelfToRenderer(this->renderer());
  this->Internal->DataRepresentations.push_back(rep);
  return rep;
}

//----------------------------------------------------------------------------
vesKiwiPlaneWidget* vesKiwiViewerApp::addPlaneWidget()
{
  vesKiwiPlaneWidget* rep = new vesKiwiPlaneWidget();
  rep->initializeWithShader(this->shaderProgram(), this->Internal->ClipUniform);
  rep->addSelfToRenderer(this->renderer());
  this->Internal->DataRepresentations.push_back(rep);
  return rep;
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::setBackgroundTexture(const std::string& filename)
{
  vesNotUsed(filename);
  //vtkSmartPointer<vtkImageData> image =
  // vtkImageData::SafeDownCast(this->Internal->DataLoader.loadDataset(filename));
  //vesTexture* backgroundTexture = this->newTextureFromImage(image);
  //this->renderer()->SetBackground(backgroundTexture);
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::setDefaultBackgroundColor()
{
  this->setBackgroundColor(63/255.0, 96/255.0, 144/255.0);
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::checkForAdditionalData(const std::string& dirname)
{
  vesNotUsed(dirname);
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::loadBrainAtlas(const std::string& filename)
{
  vesKiwiBrainAtlasRepresentation* rep = new vesKiwiBrainAtlasRepresentation();
  rep->initializeWithShader(this->shaderProgram(), this->Internal->TextureShader, this->Internal->ClipShader);
  rep->loadData(filename);
  rep->addSelfToRenderer(this->renderer());
  this->Internal->DataRepresentations.push_back(rep);

  vesKiwiPlaneWidget* planeWidget = this->addPlaneWidget();
  rep->setClipPlane(planeWidget->plane());

  this->setBackgroundColor(0., 0., 0.);
  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::loadCanSimulation(const std::string& filename)
{
  vesKiwiAnimationRepresentation* rep = new vesKiwiAnimationRepresentation();
  rep->initializeWithShader(this->shaderProgram(), this->Internal->TextureShader, this->Internal->GouraudTextureShader);
  rep->loadData(filename);
  rep->addSelfToRenderer(this->renderer());
  this->Internal->DataRepresentations.push_back(rep);
  this->setAnimating(true);
  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::loadBlueMarble(const std::string& filename)
{
  vtkSmartPointer<vtkPolyData> polyData = vtkPolyData::SafeDownCast(this->Internal->DataLoader.loadDataset(filename));
  if (!polyData) {
    this->handleLoadDatasetError();
    return false;
  }

  std::string textureFilename = vtksys::SystemTools::GetFilenamePath(filename) + "/earth.jpg";
  vtkSmartPointer<vtkImageData> image = vtkImageData::SafeDownCast(this->Internal->DataLoader.loadDataset(textureFilename));

  if (!image) {
    this->handleLoadDatasetError();
    return false;
  }

  vesKiwiPolyDataRepresentation* rep = this->addPolyDataRepresentation(polyData, this->Internal->GouraudTextureShader);
  vtkSmartPointer<vtkUnsignedCharArray> pixels = vtkUnsignedCharArray::SafeDownCast(image->GetPointData()->GetScalars());

  int width = image->GetDimensions()[0];
  int height = image->GetDimensions()[1];

  vesTexture::Ptr texture = vesTexture::Ptr(new vesTexture());
  vesKiwiDataConversionTools::SetTextureData(pixels, texture, width, height);
  rep->setTexture(texture);

  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::loadKiwiScene(const std::string& sceneFile)
{
  std::string baseDir = vtksys::SystemTools::GetFilenamePath(sceneFile);

  vtkNew<vtkDelimitedTextReader> reader;
  reader->SetFileName(sceneFile.c_str());
  reader->SetHaveHeaders(true);
  reader->Update();

  vtkSmartPointer<vtkTable> table = reader->GetOutput();

  if (!table->GetNumberOfRows()
      || !table->GetColumnByName("filename"))
    {
    this->setErrorMessage("Error reading file", "There was an error reading the file: " + sceneFile);
    return false;
    }

  const bool haveColor = (table->GetColumnByName("r")
                           && table->GetColumnByName("g")
                           && table->GetColumnByName("b"));

  const bool haveAlpha = (table->GetColumnByName("a") != NULL);

  for (int i = 0; i < table->GetNumberOfRows(); ++i) {

    std::string filename = table->GetValueByName(i, "filename").ToString();
    filename = baseDir + "/" + filename;
    std::string url;

    if (table->GetColumnByName("url")) {
      url = table->GetValueByName(i, "url").ToString();
    }

    std::cout << "filename: " << filename << std::endl;
    std::cout << "url: " << url << std::endl;

    if (!url.empty() && !vtksys::SystemTools::FileExists(filename.c_str(), true)) {
      std::string downloadedFile = this->downloadFile(url, baseDir);
      if (downloadedFile.empty()) {
        return false;
      }

      if (!this->renameFile(downloadedFile, filename)) {
        return false;
      }
    }

    std::cout << "loading: " << filename << std::endl;

    vtkSmartPointer<vtkDataSet> dataSet = this->Internal->DataLoader.loadDataset(filename);
    if (!dataSet) {
      this->handleLoadDatasetError();
      return false;
    }

    if (vtkPolyData::SafeDownCast(dataSet)) {
      vesKiwiPolyDataRepresentation* polyDataRep = this->addPolyDataRepresentation(vtkPolyData::SafeDownCast(dataSet), this->shaderProgram());

      vesVector4f color(1.0, 1.0, 1.0, 1.0);
      if (haveColor) {
        color[0] = table->GetValueByName(i, "r").ToFloat();
        color[1] = table->GetValueByName(i, "g").ToFloat();
        color[2] = table->GetValueByName(i, "b").ToFloat();
      }
      if (haveAlpha) {
        color[3] = table->GetValueByName(i, "a").ToFloat();
        if (color[3] != 1.0) {
          polyDataRep->setBinNumber(5);
        }
      }
      polyDataRep->setColor(color[0], color[1], color[2], color[3]);
    }
    else {
      this->addRepresentationsForDataSet(dataSet);
    }
  }

  return true;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::loadDatasetWithCustomBehavior(const std::string& filename)
{
  if (vtksys::SystemTools::GetFilenameName(filename) == "model_info.txt") {
    return loadBrainAtlas(filename);
  }
  else if (vtksys::SystemTools::GetFilenameName(filename) == "can0000.vtp") {
    return loadCanSimulation(filename);
  }
  if (vtksys::SystemTools::GetFilenameName(filename) == "textured_sphere.vtp") {
    return loadBlueMarble(filename);
  }
  else if (vtksys::SystemTools::GetFilenameLastExtension(filename) == ".kiwi") {
    return loadKiwiScene(filename);
  }

  return false;
}

//----------------------------------------------------------------------------
bool vesKiwiViewerApp::loadDataset(const std::string& filename)
{
  this->resetScene();

  // this is a hook that can be used to load certain datasets using custom logic
  if (this->loadDatasetWithCustomBehavior(filename)) {
    return true;
  }
  else if (!this->Internal->ErrorMessage.empty()) {
    return false;
  }

  vtkSmartPointer<vtkDataSet> dataSet = this->Internal->DataLoader.loadDataset(filename);
  if (!dataSet) {
    this->handleLoadDatasetError();
    return false;
  }

  this->addRepresentationsForDataSet(dataSet);
  return true;
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::setErrorMessage(const std::string& errorTitle, const std::string& errorMessage)
{
  if (this->Internal->ErrorMessage.empty()) {
    this->Internal->ErrorTitle = errorTitle;
    this->Internal->ErrorMessage = errorMessage;
  }
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::resetErrorMessage()
{
  this->Internal->ErrorTitle.clear();
  this->Internal->ErrorMessage.clear();
}

//----------------------------------------------------------------------------
void vesKiwiViewerApp::handleLoadDatasetError()
{
  this->setErrorMessage(this->Internal->DataLoader.errorTitle(),
                        this->Internal->DataLoader.errorMessage());
}

//----------------------------------------------------------------------------
std::string vesKiwiViewerApp::loadDatasetErrorTitle() const
{
  return this->Internal->ErrorTitle;
}

//----------------------------------------------------------------------------
std::string vesKiwiViewerApp::loadDatasetErrorMessage() const
{
  return this->Internal->ErrorMessage;
}

//----------------------------------------------------------------------------
int vesKiwiViewerApp::numberOfModelFacets() const
{
  int count = 0;
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    count += this->Internal->DataRepresentations[i]->numberOfFacets();
  }
  return count;
}

//----------------------------------------------------------------------------
int vesKiwiViewerApp::numberOfModelVertices() const
{
  int count = 0;
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    count += this->Internal->DataRepresentations[i]->numberOfVertices();
  }
  return count;
}

//----------------------------------------------------------------------------
int vesKiwiViewerApp::numberOfModelLines() const
{
  int count = 0;
  for (size_t i = 0; i < this->Internal->DataRepresentations.size(); ++i) {
    count += this->Internal->DataRepresentations[i]->numberOfLines();
  }
  return count;
}
