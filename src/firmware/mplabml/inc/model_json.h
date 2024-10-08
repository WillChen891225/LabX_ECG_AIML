#ifndef __MODEL_JSON_H__
#define __MODEL_JSON_H__

const char recognition_model_string_json[] = {"{\"NumModels\":1,\"ModelIndexes\":{\"0\":\"TEST_1_RANK_0\"},\"ModelDescriptions\":[{\"Name\":\"TEST_1_RANK_0\",\"ClassMaps\":{\"1\":\"AFib\",\"2\":\"Normal\",\"0\":\"Unknown\"},\"ModelType\":\"PME\",\"FeatureFunctions\":[\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"HarmonicProductSpectrum\",\"PeakHarmonicProductSpectrum\",\"PowerSpectrum\",\"PowerSpectrum\",\"PowerSpectrum\",\"PowerSpectrum\",\"PowerSpectrum\",\"PowerSpectrum\"]}]}"};

int32_t recognition_model_string_json_len = sizeof(recognition_model_string_json);

#endif /* __MODEL_JSON_H__ */
