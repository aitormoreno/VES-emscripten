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
/// \class vesKiwiViewerApp
/// \ingroup KiwiPlatform
#ifndef __vesKiwiViewerApp_h
#define __vesKiwiViewerApp_h

#include "vesKiwiBaseApp.h"

// VES includes
#include <vesSharedPtr.h>

// C++ includes
#include <string>

// Forward declarations
class vesCamera;
class vesKiwiDataRepresentation;
class vesKiwiPolyDataRepresentation;
class vesKiwiImagePlaneDataRepresentation;
class vesKiwiText2DRepresentation;
class vesKiwiPlaneWidget;
class vesRenderer;
class vesShaderProgram;
class vesTexture;
class vesUniform;
class vesPVWebClient;

class vtkDataSet;
class vtkPolyData;
class vtkImageData;

class vesKiwiViewerApp : public vesKiwiBaseApp
{
public:

  typedef vesKiwiBaseApp Superclass;
  vesKiwiViewerApp();
  ~vesKiwiViewerApp();

  bool doPVWebTest(const std::string& host, const std::string& sessionId);

  /// Downloads a file using cURL.
  /// Returns the absolute path to the downloaded file if successful,
  /// otherwise returns the empty string.
  std::string downloadFile(const std::string& url, const std::string& downloadDir);

  int numberOfBuiltinDatasets() const;
  int defaultBuiltinDatasetIndex() const;
  std::string builtinDatasetName(int index);
  std::string builtinDatasetFilename(int index);

  bool loadDataset(const std::string& filename);
  std::string loadDatasetErrorTitle() const;
  std::string loadDatasetErrorMessage() const;

  int  getNumberOfShadingModels() const;
  std::string getCurrentShadingModel() const;
  std::string getShadingModel(int index) const;
  bool setShadingModel(const std::string& name);

  bool initGouraudShader(const std::string& vertexSource, const std::string& fragmentSource);
  bool initBlinnPhongShader(const std::string& vertexSource, const std::string& fragmentSource);
  bool initToonShader(const std::string& vertexSource, const std::string& fragmentSource);
  bool initTextureShader(const std::string& vertexSource, const std::string& fragmentSource);
  bool initGouraudTextureShader(const std::string& vertexSource, const std::string& fragmentSource);
  bool initClipShader(const std::string& vertexSource, const std::string& fragmentSource);

  bool isAnimating() const;
  void setBackgroundTexture(const std::string& filename);

  virtual void handleSingleTouchPanGesture(double deltaX, double deltaY);
  virtual void handleSingleTouchDown(int displayX, int displayY);
  virtual void handleSingleTouchTap(int displayX, int displayY);
  virtual void handleSingleTouchUp();
  virtual void handleDoubleTap(int displayX, int displayY);
  virtual void handleLongPress(int displayX, int displayY);

  bool widgetInteractionIsActive() const;

  int numberOfModelFacets() const;
  int numberOfModelVertices() const;
  int numberOfModelLines() const;

  void checkForAdditionalData(const std::string& dirname);

  void applyBuiltinDatasetCameraParameters(int index);

  const vesSharedPtr<vesShaderProgram> shaderProgram() const;
  vesSharedPtr<vesShaderProgram> shaderProgram();


protected:

  virtual void willRender();

  virtual bool loadDatasetWithCustomBehavior(const std::string& filename);

  void addBuiltinDataset(const std::string& name, const std::string& filename);
  void addBuiltinShadingModel(
    const std::string& name, vesSharedPtr<vesShaderProgram> shaderProgram);

  void removeAllDataRepresentations();
  void addRepresentationsForDataSet(vtkDataSet* dataSet);

  void setAnimating(bool animating);

  void resetScene();

  vesKiwiPolyDataRepresentation* addPolyDataRepresentation(
    vtkPolyData* polyData, vesSharedPtr<vesShaderProgram> program);
  vesKiwiText2DRepresentation* addTextRepresentation(const std::string& text);
  vesKiwiPlaneWidget* addPlaneWidget();
  bool loadBrainAtlas(const std::string& filename);
  bool loadCanSimulation(const std::string& filename);
  bool loadBlueMarble(const std::string& filename);
  bool loadKiwiScene(const std::string& filename);
  void setDefaultBackgroundColor();

  void setErrorMessage(const std::string& errorTitle, const std::string& errorMessage);
  void resetErrorMessage();
  void handleLoadDatasetError();

  bool checkForPVWebError(vesSharedPtr<vesPVWebClient> client);

  bool renameFile(const std::string& srcFile, const std::string& destFile);

private:

  vesKiwiViewerApp(const vesKiwiViewerApp&); // Not implemented
  void operator=(const vesKiwiViewerApp&); // Not implemented

  class vesInternal;
  vesInternal* Internal;
};


#endif
