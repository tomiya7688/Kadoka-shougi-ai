#include "kadoka/training/trainer.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kadoka::shogi::training;

int main() {
    const std::vector<TrainingExample> training{
        {"sfen-a", "7g7f"},
        {"sfen-a", "7g7f"},
        {"sfen-a", "2g2f"},
        {"sfen-b", "2g2f"},
    };
    const std::vector<TrainingExample> validation{
        {"sfen-a", "7g7f"},
        {"sfen-b", "2g2f"},
        {"unseen", "3g3f"},
    };

    TrainingConfig config{987654321, 3, 2};
    const std::string config_json = serialize_training_config(config);
    assert(config_json.find("\"seed\":987654321") != std::string::npos);
    assert(config_json.find("\"epochs\":3") != std::string::npos);

    ReferenceTrainer trainer;
    assert(trainer.id() == "kadoka.reference_memorizer.v1");
    std::vector<TrainingProgress> progress_events;
    std::vector<TrainerCheckpoint> checkpoints;
    const TrainerCallbacks callbacks{
        [&progress_events](const TrainingProgress& progress) {
            progress_events.push_back(progress);
        },
        [&checkpoints](const TrainerCheckpoint& checkpoint) {
            checkpoints.push_back(checkpoint);
        },
    };

    const TrainingResult result = trainer.train(
        training,
        validation,
        config,
        callbacks
    );
    assert(result.history.size() == 3);
    assert(progress_events.size() == 3);
    assert(progress_events[0].epoch == 1);
    assert(progress_events[2].validation.samples == 3);
    assert(progress_events[2].validation.correct == 2);
    assert(progress_events[2].validation.accuracy > 0.66);
    assert(progress_events[2].validation.accuracy < 0.67);
    assert(checkpoints.size() == 2);
    assert(checkpoints[0].epoch == 2);
    assert(checkpoints[1].epoch == 3);
    assert(checkpoints[0].trainer_id == trainer.id());
    assert(checkpoints[0].payload.find("kadoka.reference_trainer_checkpoint")
        != std::string::npos);
    assert(trainer.evaluate(validation).correct == 2);

    bool invalid_config_rejected = false;
    try {
        (void)trainer.train(training, validation, TrainingConfig{0, 0, 1});
    } catch (const std::invalid_argument&) {
        invalid_config_rejected = true;
    }
    assert(invalid_config_rejected);

    bool empty_dataset_rejected = false;
    try {
        (void)trainer.train({}, validation, config);
    } catch (const std::invalid_argument&) {
        empty_dataset_rejected = true;
    }
    assert(empty_dataset_rejected);

    bool invalid_example_rejected = false;
    try {
        const std::vector<TrainingExample> bad{{"", "7g7f"}};
        (void)trainer.train(bad, {}, config);
    } catch (const std::invalid_argument&) {
        invalid_example_rejected = true;
    }
    assert(invalid_example_rejected);
}
