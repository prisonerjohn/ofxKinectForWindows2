#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
	kinect.open();
	kinect.initDepthSource();
	kinect.initColorSource();
	//kinect.initInfraredSource();
	//kinect.initBodySource();
	//kinect.initBodyIndexSource();

	mode = Mode::WorldAuto;
}

//--------------------------------------------------------------
void ofApp::update()
{
	kinect.update();
	if (kinect.isFrameNew())
	{
		auto depthSource = kinect.getDepthSource();
		auto colorSource = kinect.getColorSource();
		if (depthSource->getWidth() > 0 && colorSource->getWidth() > 0)
		{
			auto coordinateMapper = depthSource->getCoordinateMapper();

			if (mode == Mode::ColorFrame)
			{
				//mesh.clear();
				//mesh.setMode(OF_PRIMITIVE_POINTS);
				
				const int depthFrameSize = depthSource->getWidth() * depthSource->getHeight();
				const int colorFrameSize = colorSource->getWidth() * colorSource->getHeight();
				if (mesh.getNumVertices() != colorFrameSize)
				{
					mesh.setMode(OF_PRIMITIVE_POINTS);
					mesh.getVertices().resize(colorFrameSize);
					coordinateMapper->MapColorFrameToCameraSpace(depthFrameSize, depthSource->getPixels().getData(), colorFrameSize, (CameraSpacePoint *)mesh.getVerticesPointer());
				}
				else
				{
					float smoothPct = 0.5f;
					std::vector<glm::vec3> colorVertices(colorFrameSize);
					coordinateMapper->MapColorFrameToCameraSpace(depthFrameSize, depthSource->getPixels().getData(), colorFrameSize, (CameraSpacePoint *)colorVertices.data());
					auto & meshVertices = mesh.getVertices();
					for (int i = 0; i < colorFrameSize; ++i)
					{
						meshVertices[i] = meshVertices[i] * smoothPct + colorVertices[i] * (1.0f - smoothPct);
					}
				}
				
				// Get colors.
				const auto & colorPixels = colorSource->getPixels();
				mesh.getColors().resize(colorFrameSize);
				auto colors = mesh.getColorsPointer();
				const int colorWidth = colorSource->getWidth();
				for (int i = 0; i < colorFrameSize; ++i)
				{
					colors[i] = colorPixels.getColor(i % colorWidth, i / colorWidth);
				}

				ofLogNotice(__FUNCTION__) << "Color to World has " << mesh.getNumVertices() << " points VS " << colorSource->getWidth() << " x " << colorSource->getHeight() << " = " << static_cast<int>(colorSource->getWidth() * colorSource->getHeight());
			}
			else if (mode == Mode::DepthFrame)
			{
				mesh.clear();
				mesh.setMode(OF_PRIMITIVE_POINTS); 
				
				const int frameSize = depthSource->getWidth() * depthSource->getHeight();
				// Get vertices.
				mesh.getVertices().resize(frameSize);
				coordinateMapper->MapDepthFrameToCameraSpace(frameSize, depthSource->getPixels().getData(), frameSize, (CameraSpacePoint *)mesh.getVerticesPointer());
				
				// Get colors.
				std::vector<glm::vec2> texCoords(frameSize);
				coordinateMapper->MapDepthFrameToColorSpace(frameSize, depthSource->getPixels().getData(), frameSize, (ColorSpacePoint *)texCoords.data());
				const auto & colorPixels = colorSource->getPixels();
				mesh.getColors().resize(frameSize);
				auto colors = mesh.getColorsPointer();
				for (int i = 0; i < frameSize; ++i)
				{
					if (texCoords[i].x >= 0 && texCoords[i].x < colorPixels.getWidth() &&
						texCoords[i].y >= 0 && texCoords[i].y < colorPixels.getHeight())
					{
						colors[i] = colorPixels.getColor(texCoords[i].x, texCoords[i].y);
					}
					else
					{
						colors[i] = ofColor::red;
					}
				}

				ofLogNotice(__FUNCTION__) << "Depth to World has " << mesh.getNumVertices() << " points VS " << depthSource->getWidth() << " x " << depthSource->getHeight() << " = " << static_cast<int>(depthSource->getWidth() * depthSource->getHeight());
			}
			else if (mode == Mode::DepthTable)
			{
				mesh.clear();
				mesh.setMode(OF_PRIMITIVE_POINTS); 
				
				ofFloatPixels depthToWorldTable;
				depthSource->getDepthToWorldTable(depthToWorldTable);
				const int frameSize = depthSource->getWidth() * depthSource->getHeight();

				auto depthPixel = depthSource->getPixels().getData();
				auto depthToWorldRay = (glm::vec2 *)depthToWorldTable.getData();
				for (int i = 0; i < frameSize; ++i) 
				{
					auto z = (float)*depthPixel / 1000.0f;
					mesh.addVertex(glm::vec3(depthToWorldRay->x * z, depthToWorldRay->y * z, z));

					depthPixel++;
					depthToWorldRay++;
				}
			}
			else if (mode == Mode::RegisteredImage)
			{
				const auto & colorPixels = colorSource->getPixels();
				const int frameSize = depthSource->getWidth() * depthSource->getHeight();
				std::vector<glm::vec2> texCoords(frameSize);
				coordinateMapper->MapDepthFrameToColorSpace(frameSize, depthSource->getPixels().getData(), frameSize, (ColorSpacePoint *)texCoords.data());
				auto pixelData = new unsigned char[depthSource->getWidth() * depthSource->getHeight() * 3];
				for (int i = 0; i < frameSize; ++i)
				{
					ofColor color;
					if (texCoords[i].x >= 0 && texCoords[i].x < colorPixels.getWidth() &&
						texCoords[i].y >= 0 && texCoords[i].y < colorPixels.getHeight())
					{
						color = colorPixels.getColor(texCoords[i].x, texCoords[i].y);
					}
					else
					{
						color = ofColor::red;
					}
					pixelData[i * 3 + 0] = color.r;
					pixelData[i * 3 + 1] = color.g;
					pixelData[i * 3 + 2] = color.b;
				}
				image.setFromPixels(pixelData, depthSource->getWidth(), depthSource->getHeight(), OF_IMAGE_COLOR);
			}
		}
	}
}

//--------------------------------------------------------------
void ofApp::draw()
{
	ofSetColor(ofColor::white);
	if (mode == Mode::RegisteredImage)
	{
		image.draw(ofGetWidth() - image.getWidth(), 0);
	}
	else
	{
		camera.begin();
		{
			ofScale(100);
			if (mode == Mode::WorldAuto)
			{
				kinect.drawWorld();
			}
			else
			{
				mesh.draw();
			}
		}
		camera.end();
	}

	std::string modeName;
	switch (mode)
	{
	case Mode::WorldAuto:
		modeName = "World Auto";
		break;
	case Mode::ColorFrame:
		modeName = "Color Frame";
		break;
	case Mode::DepthFrame:
		modeName = "Depth Frame";
		break;
	case Mode::DepthTable:
		modeName = "Depth Table";
		break;
	case Mode::RegisteredImage:
		modeName = "Registered Image";
		break;
	}
	ofSetColor(ofColor::black);
	ofDrawBitmapString(ofToString(ofGetFrameRate(), 2) + " FPS\nMode: " + modeName, 10, 20);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
	if (key == '1')
	{
		mode = Mode::WorldAuto;
		mesh.clear();
	}
	else if (key == '2')
	{
		mode = Mode::ColorFrame;
		mesh.clear();
	}
	else if (key == '3')
	{
		mode = Mode::DepthFrame;
		mesh.clear();
	}
	else if (key == '4')
	{
		mode = Mode::DepthTable;
		mesh.clear();
	}
	else if (key == '5')
	{
		mode = Mode::RegisteredImage;
		mesh.clear();
	}
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
