/* Apple Vision framework OCR bridge — linked only on macOS with HU_ENABLE_VISION_OCR */
#if defined(__APPLE__) && defined(HU_ENABLE_VISION_OCR)

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <Vision/Vision.h>
#include "human/core/string.h"
#include "human/tools/vision_ocr.h"
#include <string.h>

hu_error_t hu_vision_ocr_recognize_apple(hu_allocator_t *alloc, const char *image_path,
                                         hu_ocr_result_t **out, size_t *out_count) {
    if (!alloc || !image_path || !out || !out_count)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_count = 0;

    @autoreleasepool {
        NSString *path = [NSString stringWithUTF8String:image_path];
        NSURL *url = [NSURL fileURLWithPath:path];
        CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
        if (!source)
            return HU_ERR_INVALID_ARGUMENT;

        CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
        CFRelease(source);
        if (!image)
            return HU_ERR_INVALID_ARGUMENT;

        CGFloat imgWidth = (CGFloat)CGImageGetWidth(image);
        CGFloat imgHeight = (CGFloat)CGImageGetHeight(image);

        __block NSMutableArray *observations = [NSMutableArray array];

        VNRecognizeTextRequest *request = [[VNRecognizeTextRequest alloc]
            initWithCompletionHandler:^(VNRequest *req, NSError *error) {
                (void)error;
                if (req.results)
                    [observations addObjectsFromArray:req.results];
            }];
        request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
        request.usesLanguageCorrection = YES;

        VNImageRequestHandler *handler = [[VNImageRequestHandler alloc] initWithCGImage:image options:@{}];
        NSError *perf_err = nil;
        (void)[handler performRequests:@[ request ] error:&perf_err];
        (void)perf_err;
        CGImageRelease(image);

        if (observations.count == 0)
            return HU_OK;

        size_t valid = 0;
        for (NSUInteger i = 0; i < observations.count; i++) {
            VNRecognizedTextObservation *obs = observations[i];
            VNRecognizedText *top = [[obs topCandidates:1] firstObject];
            if (top)
                valid++;
        }

        if (valid == 0)
            return HU_OK;

        *out = (hu_ocr_result_t *)alloc->alloc(alloc->ctx, valid * sizeof(hu_ocr_result_t));
        if (!*out)
            return HU_ERR_OUT_OF_MEMORY;
        memset(*out, 0, valid * sizeof(hu_ocr_result_t));

        size_t idx = 0;
        for (NSUInteger i = 0; i < observations.count; i++) {
            VNRecognizedTextObservation *obs = observations[i];
            VNRecognizedText *top = [[obs topCandidates:1] firstObject];
            if (!top)
                continue;

            const char *text = [top.string UTF8String];
            if (!text)
                text = "";
            (*out)[idx].text = hu_strndup(alloc, text, strlen(text));
            if (!(*out)[idx].text) {
                for (size_t j = 0; j < idx; j++) {
                    if ((*out)[j].text)
                        alloc->free(alloc->ctx, (*out)[j].text, strlen((*out)[j].text) + 1);
                }
                alloc->free(alloc->ctx, *out, valid * sizeof(hu_ocr_result_t));
                *out = NULL;
                *out_count = 0;
                return HU_ERR_OUT_OF_MEMORY;
            }

            CGRect bbox = obs.boundingBox;
            (*out)[idx].x = (double)(bbox.origin.x * imgWidth);
            (*out)[idx].y = (double)((1.0 - bbox.origin.y - bbox.size.height) * imgHeight);
            (*out)[idx].width = (double)(bbox.size.width * imgWidth);
            (*out)[idx].height = (double)(bbox.size.height * imgHeight);
            (*out)[idx].confidence = (double)obs.confidence;
            idx++;
        }

        *out_count = valid;
    }
    return HU_OK;
}

#endif /* __APPLE__ && HU_ENABLE_VISION_OCR */
