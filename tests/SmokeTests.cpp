#include <gtest/gtest.h>
#include <juce_core/juce_core.h>

TEST(Smoke, JuceCoreLinksWithoutGui)
{
    EXPECT_EQ(juce::String("tap") + "Synth", juce::String("tapSynth"));
}

TEST(Smoke, JuceJsonParsesGeminiLikeResponse)
{
    const juce::var v = juce::JSON::parse(
        R"({"candidates":[{"content":{"parts":[{"text":"{}"}]}}]})");

    ASSERT_TRUE(v.isObject());
    EXPECT_TRUE(v["candidates"].isArray());
}
