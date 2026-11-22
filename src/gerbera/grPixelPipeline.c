/* grPixelPipeline.c - the pixel-pipe dispatch (port of grPixelPipeline.h).
 *
 * The compiled program (state->pixelPipelineCode, produced by the hc
 * shader compiler) consumes the pixel-pipe record at
 * renderStack+14644: the row pointers (+168..+176), scanline state
 * (+180..+188) and the gradient arrays set up by grSetupTriangleGradients.
 */
#include "gerbera/grState.h"
#include "gerbera/grDefs.h"
#include "hg/hgInt32.h"

/* --- grExecutePixelPipeline (grPixelPipeline.h 153; DLL 24757-24775) --------- */

void grExecutePixelPipeline(Gerbera_s *state)
{
    grPixelPipelineCode_fn pixelPipelineCode = state->pixelPipelineCode;

    if (pixelPipelineCode == NULL &&
        grAssert("pixelPipelineCode", "grPixelPipeline.h", 153) != 0)
        grDie(0);
    /* the decompile reaches this through renderStack->pixelPipelineStack -
     * the pixel-pipe pointer living at renderStack+14644 (the same address
     * passed to the program); state->pixelPipeline is that pointer */
    if (state->pixelPipeline == NULL &&
        grAssert("state->renderStack->pixelPipelineStack", "grPixelPipeline.h", 154) != 0)
        grDie(0);
    pixelPipelineCode(state->pixelPipeline);
}
