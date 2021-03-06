#include "pch.h"

#include "ImageAnalyzer.h" // Own header

#include "ImageProcessingUtils.h"
#include "ObjectDetails.h"


// Constants
const double TwoPi = Pi * 2;
const double AngleIncrement = 0.1667 * Pi; // Roughly 1/6 * pi


ImageAnalyzer::ImageAnalyzer(ImageProcessingUtils* imageProcessingUtils)
	: m_imageProcessingUtils(imageProcessingUtils)
{
}


ImageAnalyzer::~ImageAnalyzer()
{
}


//------------------------------------------------------------------
// extractObjectDetails
//
// Extracts object details from the given, organized object map.
//------------------------------------------------------------------
std::vector<ObjectDetails*> ImageAnalyzer::extractObjectDetails(
	const UINT16* organizedObjectMap, const UINT32& objectMapWidth, const UINT32& objectMapHeight, const UINT16& objectCount)
{
	std::vector<ObjectDetails*> objectDetailsVector;
	UINT32* objectPixelCountOnXAxis = NULL;

	for (UINT16 objectId = 1; objectId <= objectCount; ++objectId)
	{
		ObjectDetails *newObjectDetails = new ObjectDetails();
		newObjectDetails->_id = objectId;

		UINT32 currentIndex = 0;
		UINT32 objectPixelCountOnYAxis = 0;
		objectPixelCountOnXAxis = new UINT32[objectMapWidth];

		for (UINT32 i = 0; i < objectMapWidth; ++i)
		{
			objectPixelCountOnXAxis[i] = 0;
		}

		for (UINT32 y = 0; y < objectMapHeight; ++y)
		{
			objectPixelCountOnYAxis = 0;

			for (UINT32 x = 0; x < objectMapWidth; ++x)
			{
				if (organizedObjectMap[currentIndex] == objectId)
				{
					objectPixelCountOnYAxis++;
					objectPixelCountOnXAxis[x]++;
					newObjectDetails->_area++;
				}

				currentIndex++;
			}

			if (newObjectDetails->_width < objectPixelCountOnYAxis)
			{
				newObjectDetails->_width = objectPixelCountOnYAxis;
				newObjectDetails->_centerY = y;
			}
		}

		for (UINT32 i = 0; i < objectMapWidth; ++i)
		{
			if (objectPixelCountOnXAxis[i] > newObjectDetails->_height)
			{
				newObjectDetails->_height = objectPixelCountOnXAxis[i];
				newObjectDetails->_centerX = i;
			}
		}

		delete objectPixelCountOnXAxis;
		objectDetailsVector.push_back(newObjectDetails);
	}

	return objectDetailsVector;
}


//------------------------------------------------------------------
// resolveLargeObjectIds
//
// Resolves the object IDs of all objects whose width or height is
// equal or greater than the given minimum size.
//------------------------------------------------------------------
std::vector<UINT16>* ImageAnalyzer::resolveLargeObjectIds(
	const UINT16* organizedObjectMap,
    const UINT32& objectMapWidth, const UINT32& objectMapHeight,
    UINT16& objectCount, const UINT32& minSize)
{
	std::vector<ObjectDetails*> objectDetails =
        extractObjectDetails(organizedObjectMap, objectMapWidth, objectMapHeight, objectCount);
	sortObjectDetailsBySize(objectDetails);
	const UINT16 objectDetailsCount = objectDetails.size();
	std::vector<UINT16>* largeObjectIds = new std::vector<UINT16>();

	for (UINT16 i = 0; i < objectDetailsCount; ++i)
	{
		ObjectDetails* currentObjectDetails = objectDetails.at(i);

		if (currentObjectDetails
			&& currentObjectDetails->_width >= minSize
			/*&& currentObjectDetails->_height >= minSize*/)
		{
			largeObjectIds->push_back(objectDetails.at(i)->_id);
		}
	}
	
	return largeObjectIds;
}


//------------------------------------------------------------------
// extractConvexHullsOfLargestObjects
//
// Returns a vector of newly allocated convex hulls.
//------------------------------------------------------------------
std::vector<ConvexHull*> ImageAnalyzer::extractConvexHullsOfLargestObjects(
	BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight,
	const UINT8& maxNumberOfConvexHulls, const IMAGE_TRANSFORM_FUNCTION &pTransformFn)
{
	std::vector<ConvexHull*> convexHulls;

	UINT16* objectMap = m_imageProcessingUtils->createObjectMap(binaryImage, imageWidth, imageHeight, pTransformFn);

	if (objectMap)
	{
		UINT16 objectCount = m_imageProcessingUtils->organizeObjectMap(objectMap, imageWidth * imageHeight);
		UINT32 minSize = (UINT32)((float)imageWidth * RelativeObjectSizeThreshold);
		std::vector<UINT16>* largeObjectIds = resolveLargeObjectIds(objectMap, imageWidth, imageHeight, objectCount, minSize);
		objectCount = largeObjectIds->size();
		std::vector<D2D_POINT_2U>* sortedPoints = NULL;
		ConvexHull* convexHull = NULL;

		for (UINT16 i = 0; i < objectCount && convexHulls.size() <= maxNumberOfConvexHulls; ++i)
		{
			sortedPoints = m_imageProcessingUtils->extractSortedObjectPoints(objectMap, imageWidth, imageHeight, largeObjectIds->at(i));

			if (sortedPoints)
			{
				convexHull = m_imageProcessingUtils->createConvexHull(*sortedPoints, true);
				convexHulls.push_back(convexHull);
				delete sortedPoints;
			}
		}

		delete largeObjectIds;
	}

	delete objectMap;

	return convexHulls;
}


//------------------------------------------------------------------
// convexHullClosestToCircle
//
//
//------------------------------------------------------------------
ConvexHull* ImageAnalyzer::convexHullClosestToCircle(const std::vector<ConvexHull*> &convexHulls, LONG &error)
{
	const UINT16 convexHullCount = convexHulls.size();
	ConvexHull* convexHull = NULL;
	UINT32 width = 0;
	UINT32 height = 0;
	D2D_POINT_2U center;
	LONG currentError = 0;
	error = -1;
	int bestIndex = -1;

	for (UINT16 i = 0; i < convexHullCount; ++i)
	{
		convexHull = convexHulls.at(i);

		if (convexHull)
		{
			calculateConvexHullDimensions(*convexHull, width, height, center.x, center.y);
			currentError = circleCircumferenceError(*convexHull, width, height, center);

			if (currentError < error || error < 0)
			{
				error = currentError;
				bestIndex = i;
			}
		}
	}

	if (bestIndex >= 0)
	{
		return convexHulls.at(bestIndex);
	}

	return NULL;
}


//------------------------------------------------------------------
// calculateConvexHullDimensions
//
// Calculates the width, height and the center point of the given
// convex hull.
//------------------------------------------------------------------
inline void ImageAnalyzer::calculateConvexHullDimensions(
    const ConvexHull& convexHull, UINT32& width, UINT32& height, UINT32& centerX, UINT32& centerY)
{
    const UINT32 convexHullSize = convexHull.size();

    if (convexHullSize < 2)
    {
        return;
    }

    D2D_POINT_2U currentPoint = convexHull.at(0);
    UINT32 minX = currentPoint.x;
    UINT32 maxX = currentPoint.x;
    UINT32 minY = currentPoint.y;
    UINT32 maxY = currentPoint.y;

    for (UINT32 i = 1; i < convexHullSize; ++i)
    {
        currentPoint = convexHull.at(i);

        if (currentPoint.x < minX)
        {
            minX = currentPoint.x;
        }
        else if (currentPoint.x > maxX)
        {
            maxX = currentPoint.x;
        }

        if (currentPoint.y < minY)
        {
            minY = currentPoint.y;
        }
        else if (currentPoint.y > maxY)
        {
            maxY = currentPoint.y;
        }
    }

    width = maxX - minX;
    height = maxY - minY;
    centerX = (maxX + minX) / 2;
    centerY = (maxY + minY) / 2;
}


//------------------------------------------------------------------
// convexHullDimensionsAsObjectDetails
//
//
//------------------------------------------------------------------
ObjectDetails* ImageAnalyzer::convexHullDimensionsAsObjectDetails(const ConvexHull& convexHull)
{
	ObjectDetails* objectDetails = new ObjectDetails();

	calculateConvexHullDimensions(convexHull,
		objectDetails->_width, objectDetails->_height,
		objectDetails->_centerX, objectDetails->_centerY);

	return objectDetails;
}


//------------------------------------------------------------------
// circleCircumferenceError
//
// This method does not calculate the error of the whole
// circumference. Instead, it takes N number of points on the
// circumference of a circle and compares them to the closest points
// found on the convex hull.
//
// Note: The method does not handle situations where the convex hull
// does not contain the full object (e.g. only half of the ball is
// visible).
//------------------------------------------------------------------
LONG ImageAnalyzer::circleCircumferenceError(
	const ConvexHull& convexHull, const UINT32& convexHullWidth, const UINT32& convexHullHeight,
	const D2D_POINT_2U& convexHullCenter)
{
	UINT32 radius = (UINT32)round(((double)convexHullWidth + (double)convexHullHeight) / 4);
	D2D_POINT_2L pointOnPerfectCircleCircumference;
	LONG pointError = -1;
	LONG tempError = 0;
	LONG totalError = 0;
	const int convexHullPointCount = convexHull.size();
	D2D_POINT_2U currentPointInConvexHull;

	for (double angle = 0; angle < TwoPi; angle += AngleIncrement)
	{
		getPointOnCircumference(convexHullCenter, radius, angle, pointOnPerfectCircleCircumference);

		pointError = -1;

		for (int i = 0; i < convexHullPointCount; ++i)
		{
			currentPointInConvexHull = convexHull.at(i);

			// Manhattan distance is faster than euclidean, and does not affect the
			// outcome since our scale of error is arbitrary.
			tempError =
				abs((LONG)currentPointInConvexHull.x - pointOnPerfectCircleCircumference.x)
				+ abs((LONG)currentPointInConvexHull.y - pointOnPerfectCircleCircumference.y);

			tempError /= (convexHullWidth + convexHullHeight) / 2; // Normalize

            if (convexHullHeight != 0)
            {
                tempError *= (convexHullWidth / convexHullHeight);
            }

			if (pointError < tempError || pointError < 0)
			{
				pointError = tempError;
			}
		}

		totalError += pointError;
	}

	return totalError;
}


//------------------------------------------------------------------
// circleAreaError
//
// Calculates the error between the area of a perfect circle (if it
// had the given diameter) and the given measured area.
// The calculated error is the difference between the perfect and
// the measured area as an absolute value.
//------------------------------------------------------------------
double ImageAnalyzer::circleAreaError(UINT32 measuredDiameter, UINT32 measuredArea)
{
	float radius = static_cast<float>(measuredDiameter) / 2;
	return abs(measuredArea - Pi *  radius * radius);
}


//-------------------------------------------------------------------
// bestConvexHullDetails
//
//
//-------------------------------------------------------------------
ConvexHull* ImageAnalyzer::bestConvexHullDetails(
	BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight, const UINT8& maxCandidates, const IMAGE_TRANSFORM_FUNCTION &pTransformFn)
{
	std::vector<ConvexHull*> convexHulls =
		extractConvexHullsOfLargestObjects(binaryImage, imageWidth, imageHeight, maxCandidates, pTransformFn);

	if (convexHulls.size() > 0)
	{
		std::vector<D2D_POINT_2U>* convexHull = NULL;
		LONG error = 0;

		for (UINT16 i = 0; i < convexHulls.size(); ++i)
		{
			convexHull = convexHulls.at(i);

			for (UINT32 i = 0; i < convexHull->size() - 1; ++i)
			{
				m_imageProcessingUtils->drawLine(
					binaryImage, imageWidth, imageHeight,
					convexHull->at(i), convexHull->at(i + 1), pTransformFn, 3, 0x80, 0x60, 0xff);
			}
		}

		convexHull = convexHullClosestToCircle(convexHulls, error);

		if (convexHull)
		{
			for (UINT32 i = 0; i < convexHull->size() - 1; ++i)
			{
				m_imageProcessingUtils->drawLine(
					binaryImage, imageWidth, imageHeight,
					convexHull->at(i), convexHull->at(i + 1), pTransformFn, 3, 0x80, 0x10, 0x10);
			}

			// Delete other convex hulls
			for (UINT16 i = 0; i < convexHulls.size(); ++i)
			{
				if (convexHulls.at(i) != convexHull)
				{
					delete convexHulls.at(i);
					convexHulls[i] = NULL;
				}
			}

			return convexHull;
		}

		DeletePointerVector(convexHulls);
	}

	return NULL;
}


//-------------------------------------------------------------------
// objectIsWithinConvexHullBounds
//
//
//-------------------------------------------------------------------
bool ImageAnalyzer::objectIsWithinConvexHullBounds(const ObjectDetails& objectDetails, const ConvexHull& convexHull)
{
	ObjectDetails* convexHullObjectDetails = convexHullDimensionsAsObjectDetails(convexHull);

	bool isWithin = true;

	if (abs(static_cast<double>(convexHullObjectDetails->_centerX) - objectDetails._centerX) > convexHullObjectDetails->_width / 2
		|| abs(static_cast<double>(convexHullObjectDetails->_centerY) - objectDetails._centerY) > convexHullObjectDetails->_height / 2)
	{
		// The object details center is outside of the convex hull
		isWithin = false;
	}

	delete convexHullObjectDetails;
	return isWithin;
}


//------------------------------------------------------------------
// getPointOnCircumference
//
// Calculates a point on circle's circumference based on the given
// center, radius and angle.
//------------------------------------------------------------------
void ImageAnalyzer::getPointOnCircumference(const D2D_POINT_2U& center, const UINT32& radius, const double& angle, D2D_POINT_2L& point)
{
#pragma warning(push)
#pragma warning(disable: 4244) 
	point.x = center.x + (LONG)radius * cos(angle);
	point.y = center.y + (LONG)radius * sin(angle);
#pragma warning(pop)
}


//------------------------------------------------------------------
// sortObjectDetailsBySize
//
// 
//------------------------------------------------------------------
void ImageAnalyzer::sortObjectDetailsBySize(std::vector<ObjectDetails*> &objectDetails)
{
	std::sort(objectDetails.begin(), objectDetails.end(),
		[](ObjectDetails *a, ObjectDetails *b)
	{
		if (a->_width * a->_height > b->_width * b->_height)
		{
			return true;
		}

		return false;
	});
}

